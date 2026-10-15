#include "bist.hpp"
#include "hardware.hpp"
#include "metadata.hpp"
#include "uart.hpp"
#include "checkpoint.hpp"
#include "pacing.hpp"
#include "upload.hpp"
#include <charconv>
#include <drivers_json.hpp>
#include <atomic>
#include <barrier>
#include <filesystem>
#include <mutex>
#include <thread>
#include <array>
#include <numbers>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/utsname.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctime>
#include <cstdio>
#include <cerrno>
#include <cstring>

namespace
{
using namespace bist;
constexpr auto root = "/var/lib/koheron/bist";
constexpr unsigned workload_version = 3;
constexpr double unavailable = std::numeric_limits<double>::quiet_NaN();
struct Metric
{
    std::string name, domain, unit, detail, extra;
    double value;
    bool pass;
    bool headline;
    std::string status;
    std::string json() const
    {
        return "{\"name\":" + quote(name) + ",\"domain\":" + quote(domain) +
               ",\"unit\":" + quote(unit) + ",\"value\":" + number(value) +
               ",\"pass\":" + (pass ? "true" : "false") + ",\"status\":" + quote(status) +
               ",\"headline\":" + (headline ? "true" : "false") + ",\"detail\":" + quote(detail) +
               (extra.empty() ? "" : "," + extra) + "}";
    }
};
std::string timestamp()
{
    timespec now{};
    clock_gettime(CLOCK_REALTIME, &now);
    return utc_timestamp(now.tv_sec, now.tv_nsec);
}
std::string digest(const std::string& path)
{
    // Callers supply fixed instrument paths or our numeric /proc PID path.
    const std::string command = "sha256sum " + path + " 2>/dev/null";
    auto* pipe = popen(command.c_str(), "r");
    if (!pipe)
        return "unavailable";
    char buffer[128]{};
    const auto* result = fgets(buffer, sizeof(buffer), pipe);
    pclose(pipe);
    return result ? std::string(buffer, 64) : "unavailable";
}
size_t resource_count(const char* path)
{
    std::error_code error;
    size_t count = 0;
    for (std::filesystem::directory_iterator it(path, error), end; !error && it != end;
         it.increment(error))
        ++count;
    return count;
}
bool socket_io(int fd, void* data, size_t size, bool write)
{
    auto* p = static_cast<char*>(data);
    while (size)
    {
        const auto done = write ? ::send(fd, p, size, MSG_NOSIGNAL) : ::recv(fd, p, size, 0);
        if (done < 0 && errno == EINTR)
            continue;
        if (done == 0)
            errno = write ? EPIPE : ECONNRESET;
        if (done <= 0)
            return false;
        p += done;
        size -= static_cast<size_t>(done);
    }
    return true;
}
uint32_t rpc_operation_named(std::string_view requested)
{
    using Adapter = koheron::adapter_meta<koheron::BistAdapter>;
    uint32_t operation = std::numeric_limits<uint32_t>::max();
    [&]<size_t... I>(std::index_sequence<I...>)
    {
        ((
             [&]
             {
                 using Operation = std::tuple_element_t<I, typename Adapter::ops_tuple>;
                 if (koheron::op_name<koheron::op_meta<Operation>::pmf>() == requested)
                     operation = (uint32_t(Adapter::id) << 16) | uint32_t(I);
             }()),
         ...);
    }(std::make_index_sequence<std::tuple_size_v<typename Adapter::ops_tuple>>{});
    return operation;
}
uint32_t rpc_operation(bool bulk) { return rpc_operation_named(bulk ? "benchmark_reply" : "ping"); }
struct RpcResult
{
    uint64_t calls = 0, errors = 0, bytes = 0;
    double elapsed = 0;
    LatencyHistogram us;
    uint64_t late_calls = 0;
    std::string diagnostic;
};
RpcResult rpc_benchmark(bool bulk, double duration, std::atomic<bool>& cancel,
                        CombinedLoad* load = nullptr)
{
    RpcResult result;
    auto drop = [&]
    {
        if (load)
            load->ready.drop();
    };
    const int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0)
    {
        result.errors = 1;
        drop();
        result.diagnostic = "TCP socket: " + std::string(std::strerror(errno));
        return result;
    }
    timeval timeout{1, 0};
    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(36000);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (::connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0)
    {
        result.errors = 1;
        drop();
        result.diagnostic = "TCP connect: " + std::string(std::strerror(errno));
        ::close(fd);
        return result;
    }
    const uint32_t operation = rpc_operation(bulk);
    std::array<uint32_t, 3> request{0, htonl(operation), htonl(bulk ? 1024U : 0x13579bdfU)};
    std::vector<uint32_t> response(bulk ? 1024 : 1);
    if (load)
    {
        load->rpc_affinity_error = pin_worker(0);
        // Warm the generated dispatch and reply path before the common epoch.
        std::array<uint32_t, 2> header{};
        uint32_t reply = 0;
        if (!socket_io(fd, request.data(), 12, true) || !socket_io(fd, header.data(), 8, false) ||
            !socket_io(fd, &reply, 4, false) || header[0] != 0 || ntohl(header[1]) != operation ||
            ntohl(reply) != (0x13579bdfU ^ 0x42495354U))
        {
            drop();
            result.errors = 1;
            result.diagnostic = "RPC warmup failed";
            ::close(fd);
            return result;
        }
        load->ready.wait();
        load->rpc_start_us = seconds(load->ready.start()) * 1e6;
    }
    const auto start = load ? load->ready.start() : Clock::now();
    while (!cancel &&
           (load ? (!load->serial_done || seconds(start) < duration) : seconds(start) < duration))
    {
        const auto call_start = Clock::now();
        std::array<uint32_t, 3> header{};
        if (!socket_io(fd, request.data(), 12, true))
        {
            ++result.errors;
            result.diagnostic = "RPC request send: " + std::string(std::strerror(errno));
            break;
        }
        if (!socket_io(fd, header.data(), bulk ? 12 : 8, false))
        {
            ++result.errors;
            result.diagnostic = "RPC reply header: " + std::string(std::strerror(errno));
            break;
        }
        if (header[0] != 0 || ntohl(header[1]) != operation || (bulk && ntohl(header[2]) != 4096))
        {
            ++result.errors;
            result.diagnostic = "RPC reply header identity or payload length mismatch";
            break;
        }
        if (!socket_io(fd, response.data(), response.size() * 4, false))
        {
            ++result.errors;
            result.diagnostic = "RPC reply payload: " + std::string(std::strerror(errno));
            break;
        }
        if (bulk)
        {
            for (size_t i = 0; i < response.size(); ++i)
                result.errors += response[i] != pattern(uint32_t(i), 0x42495354U);
        }
        else
            result.errors += ntohl(response[0]) != (0x13579bdfU ^ 0x42495354U);
        if (result.errors != 0)
            result.diagnostic = "RPC reply data mismatch";
        const bool sampled = !load || call_start < load->window_end();
        if (sampled)
        {
            result.us.add(seconds(call_start) * 1e6);
            if (load && Clock::now() > load->window_end())
                ++result.late_calls;
            else
            {
                ++result.calls;
                result.bytes += response.size() * 4;
            }
        }
    }
    result.elapsed = load ? duration : seconds(start);
    ::close(fd);
    return result;
}
// Separate probe so the established saturation workload retains its timing.
PacedResult controlled_rpc(StartGate& gate, std::atomic<bool>& cancelled, unsigned case_index)
{
    PacedResult result;
    result.affinity_error = pin_worker(0);
    const int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    timeval timeout{1, 0};
    int one = 1;
    if (fd >= 0)
    {
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
        setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    }
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(36000);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    const bool upload = case_index >= 4;
    const uint32_t operation =
        upload ? rpc_operation_named(case_index == 4 ? "benchmark_upload"
                                                     : "benchmark_upload_reference")
               : rpc_operation(false);
    std::array<std::vector<uint32_t>, 2> packets;
    if (upload)
        for (size_t i = 0; i < packets.size(); ++i)
            packets[i] = upload_request(operation, upload_seeds[i]);
    uint32_t nonce = 0x13579bdf;
    unsigned sequence = 0;
    auto call = [&]
    {
        std::array<uint32_t, 3> request{0, htonl(operation), htonl(++nonce)}, reply{};
        uint32_t expected = nonce ^ 0x42495354U;
        uint32_t* data = request.data();
        size_t bytes = sizeof(request);
        if (upload)
        {
            const auto index = sequence++ % packets.size();
            data = packets[index].data();
            bytes = packets[index].size() * sizeof(uint32_t);
            expected = upload_seeds[index] ^ 0x42495354U ^ (upload_bytes / sizeof(uint32_t));
        }
        return socket_io(fd, data, bytes, true) && socket_io(fd, reply.data(), 8, false) &&
               socket_io(fd, reply.data() + 2, 4, false) && reply[0] == 0 &&
               ntohl(reply[1]) == operation && ntohl(reply[2]) == expected;
    };
    if (fd < 0 || ::connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0 ||
        !call())
    {
        result.errors = 1;
        gate.drop();
    }
    else
    {
        gate.wait();
        paced(result, gate.start(), upload ? upload_requests_per_second : comparison_rpc_rate,
              comparison_seconds, cancelled, call);
    }
    if (fd >= 0)
        ::close(fd);
    return result;
}
} // namespace

struct Bist::Impl
{
    hw::Memory<mem::control>& ctl = hw::get_memory<mem::control>();
    hw::Memory<mem::status>& sts = hw::get_memory<mem::status>();
    hw::Memory<mem::bram>& bram = hw::get_memory<mem::bram>();
    hw::Memory<mem::buffers>& ram = hw::get_memory<mem::buffers>();
    hw::Memory<mem::dma>& dma_regs = hw::get_memory<mem::dma>();
    std::atomic<bool> cancelled{false}, active{false};
    std::thread worker;
    std::mutex mutex;
    std::string run_id, prefix, metrics = "[]", metadata = "{}", state = "idle", log_text,
                                metadata_text;
    std::vector<Metric> values;
    std::vector<uint32_t> upload_storage;
    bool storage_ok = true;
    bool defer_io = false;
    Profile profile;
    unsigned comparison_mask = 15;
    std::atomic<bool> measuring{false};
    std::atomic<uint64_t> observer_requests{0};
    std::string functional = "unavailable", performance_status = "not_evaluated", performance;
    std::string scorecard;
    Clock::time_point beginning;
    Dma dma{dma_regs, ram, ctl, cancelled};

    Impl()
    {
        run_id = read_file(std::string(root) + "/latest");
        if (!run_id.empty() && run_id.find_first_not_of("0123456789-") == std::string::npos)
        {
            prefix = std::string(root) + "/" + run_id;
            Checkpoint saved;
            bool restored = decode_checkpoint(read_file(prefix + "/checkpoint"), saved);
            if (!restored)
            {
                restored = decode_checkpoint(read_file(prefix + "/checkpoint.prev"), saved);
                storage_ok = false;
            }
            if (restored)
            {
                metrics = saved[0];
                metadata = saved[1];
                if (metadata.find("\"profile\":\"comparison\"") != std::string::npos)
                {
                    profile.id = 3;
                    const auto key = metadata.find("\"case_mask\":");
                    if (key != std::string::npos)
                        std::from_chars(metadata.data() + key + 12,
                                        metadata.data() + metadata.size(), comparison_mask);
                }
                metadata_text = saved[2];
                state = saved[3];
                log_text = saved[4];
                scorecard = saved[5];
                functional = saved[6];
                performance_status = saved[7];
                performance = saved[8];
                if (state == "running")
                {
                    state = "interrupted";
                    functional = "incomplete";
                    persist();
                }
            }
            else
            {
                state = "corrupt";
                functional = "unavailable";
                log_text = "No valid checkpoint; partial exports are not restored.\n";
            }
        }
        else
            run_id.clear();
    }
    ~Impl()
    {
        cancelled = true;
        if (worker.joinable())
            worker.join();
        dma.stop();
    }
    bool write_file(const std::string& path, std::string_view contents)
    {
        std::ofstream output(path + ".tmp", std::ios::binary | std::ios::trunc);
        output << contents;
        output.close();
        if (!output)
        {
            storage_ok = false;
            return false;
        }
        std::error_code error;
        std::filesystem::rename(path + ".tmp", path, error);
        if (error)
            storage_ok = false;
        return !error;
    }
    std::string report_unlocked() const
    {
        constexpr std::array<std::string_view, 33> required{"uio_mapping",
                                                            "write_combined_mapping",
                                                            "read_only_mapping",
                                                            "dma_reservation",
                                                            "converter_clock",
                                                            "mapping_lifetime",
                                                            "register_read",
                                                            "register_write",
                                                            "bram_write",
                                                            "bram_read",
                                                            "dma_uio_interrupt_binding",
                                                            "dma_isolated",
                                                            "uart_isolated",
                                                            "uart_roundtrip_isolated",
                                                            "uart_baud",
                                                            "uio_observation",
                                                            "rpc_isolated",
                                                            "rpc_bulk",
                                                            "waveform_upload",
                                                            "adc_capture",
                                                            "combined_window",
                                                            "analog_0",
                                                            "analog_1",
                                                            "rpc_combined",
                                                            "dma_combined",
                                                            "uart_combined",
                                                            "uart_roundtrip_combined",
                                                            "cpu_memory_load",
                                                            "suite_duration",
                                                            "network_readiness",
                                                            "server_readiness",
                                                            "boot_readiness",
                                                            "instrument_restart"};
        std::string missing = "[";
        std::vector<std::string> expected;
        if (profile.id == 3)
        {
            for (unsigned i = 0; i < comparison_cases.size(); ++i)
                if (comparison_mask & (1U << i))
                    expected.push_back("rpc_controlled_" + std::string(comparison_cases[i]));
            expected.push_back("suite_duration");
        }
        else
            for (const auto name : required)
                expected.emplace_back(name);
        for (const auto& name : expected)
        {
            if (metrics.find("\"name\":" + quote(name)) != std::string::npos)
                continue;
            if (missing.size() > 1)
                missing += ',';
            missing += quote(name);
        }
        missing += ']';
        return "{\"schema_version\":1,\"run_id\":" + quote(run_id) + ",\"state\":" + quote(state) +
               ",\"storage_ok\":" + (storage_ok ? "true" : "false") + ",\"metadata\":" + metadata +
               ",\"functional_status\":" + quote(state == "running" ? "running" : functional) +
               ",\"performance\":{\"status\":" + quote(performance_status) +
               ",\"comparison_json\":" + quote(performance) + "},\"metrics\":" + metrics +
               ",\"missing_measurements\":" + missing + "}";
    }
    void persist()
    {
        if (prefix.empty() || defer_io)
            return;
        write_file(prefix + "/metrics.json", metrics);
        write_file(prefix + "/metadata.json", metadata);
        write_file(prefix + "/metadata.txt", metadata_text);
        write_file(prefix + "/state", state);
        write_file(prefix + "/log.txt", log_text);
        write_file(prefix + "/report.json", report_unlocked());
        std::ostringstream card;
        card << "Red Pitaya BIST | run " << run_id << " | " << state << "\n"
             << "Functional: " << functional << " | Performance: " << performance_status << "\n"
             << metadata_text;
        if (!values.empty() || state == "running")
        {
            for (const auto& value : values)
                card << value.status << " | " << value.domain << " | " << value.name << " | "
                     << number(value.value) << ' ' << value.unit << " | " << value.detail << '\n';
            scorecard = card.str();
        }
        else
        {
            auto body = scorecard.find('\n');
            if (body != std::string::npos)
                body = scorecard.find('\n', body + 1);
            scorecard = "Red Pitaya BIST | run " + run_id + " | " + state +
                        "\nFunctional: " + functional + " | Performance: " + performance_status +
                        "\n" + (body == std::string::npos ? "" : scorecard.substr(body + 1));
        }
        write_file(prefix + "/scorecard.txt", scorecard);
        const auto encoded =
            encode_checkpoint({metrics, metadata, metadata_text, state, log_text, scorecard,
                               functional, performance_status, performance});
        // The previous committed generation remains recoverable even if killed
        // between these renames. Never restore independent exports together.
        if (write_file(prefix + "/checkpoint.next", encoded))
        {
            std::error_code error;
            if (std::filesystem::exists(prefix + "/checkpoint", error))
                std::filesystem::rename(prefix + "/checkpoint", prefix + "/checkpoint.prev", error);
            if (!error)
                std::filesystem::rename(prefix + "/checkpoint.next", prefix + "/checkpoint", error);
            if (error)
                storage_ok = false;
            if (!error && state != "running")
            {
                // Terminal reports survive a later reboot; timed regions never sync
                // files.
                const int file = ::open((prefix + "/checkpoint").c_str(), O_RDONLY | O_CLOEXEC);
                const int directory = ::open(prefix.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
                if (file < 0 || directory < 0)
                    storage_ok = false;
                if (file >= 0)
                {
                    if (::fsync(file) != 0)
                        storage_ok = false;
                    ::close(file);
                }
                if (directory >= 0)
                {
                    if (::fsync(directory) != 0)
                        storage_ok = false;
                    ::close(directory);
                }
            }
        }
    }
    std::string log_stamp() const
    {
        return timestamp() + " steady_ms=" +
               number(std::chrono::duration<double, std::milli>(Clock::now().time_since_epoch())
                          .count());
    }
    void log(std::string text)
    {
        std::lock_guard lock(mutex);
        log_text += log_stamp() + " " + text + "\n";
        persist();
    }
    void add(std::string name, double value, std::string unit, bool pass, std::string detail,
             std::string domain = "os", std::string extra = "", bool headline = true,
             std::string status = "")
    {
        std::lock_guard lock(mutex);
        Metric metric{std::move(name),
                      std::move(domain),
                      std::move(unit),
                      std::move(detail),
                      std::move(extra),
                      value,
                      pass,
                      headline,
                      status.empty() ? (cancelled ? "incomplete"
                                        : pass    ? "pass"
                                                  : "fail")
                                     : status};
        if (metric.status == "incomplete")
        {
            metric.value = unavailable;
            metric.pass = false;
            functional = "incomplete";
        }
        else if (!metric.pass)
            functional = "fail";
        // Append to the persisted JSON array, also after instrument reconstruction.
        metrics.pop_back();
        if (metrics.size() > 1)
            metrics += ",";
        metrics += metric.json() + "]";
        if (values.empty() && state != "running")
        {
            const auto old = read_file(prefix + "/scorecard.txt");
            scorecard = old + metric.status + " " + metric.domain + " | " + metric.name + " | " +
                        number(value) + " " + metric.unit + " | " + metric.detail + "\n";
        }
        else
            values.push_back(metric);
        log_text += log_stamp() + " " + metric.status + " " + metric.name +
                    " expected=" + metric.detail + " observed=" + number(value) + " " +
                    metric.unit + "\n";
        persist();
    }
    bool snapshot()
    {
        const uint32_t next = ctl.read<reg::snapshot>() + 1;
        const uint32_t id = next == 0 ? 1 : next;
        ctl.write<reg::snapshot>(id);
        barrier();
        const auto deadline = Clock::now() + std::chrono::milliseconds(20);
        while (sts.read<reg::snapshot_id>() != id && Clock::now() < deadline)
            std::this_thread::yield();
        return sts.read<reg::snapshot_id>() == id;
    }
    void mappings()
    {
        const std::array<std::pair<std::string, uintptr_t>, 7> regions{
            {{"control", ctl.base_addr()},
             {"status", sts.base_addr()},
             {"dma", dma_regs.base_addr()},
             {"uart_a", hw::get_memory<mem::uart_a>().base_addr()},
             {"uart_b", hw::get_memory<mem::uart_b>().base_addr()},
             {"bram", bram.base_addr()},
             {"buffers", ram.base_addr()}}};
        bool uio_ok = true, wc_ok = true, readonly = false;
        std::string detail;
        for (const auto& [name, base] : regions)
        {
            const auto path = mapping_path(base);
            detail += name + "=" + path + "; ";
            if (name == "bram" || name == "buffers")
                wc_ok &= path.starts_with("/dev/mem_wc");
            else
                uio_ok &= path.starts_with("/dev/uio");
            if (name == "status")
                readonly = path.find(" r--s") != std::string::npos;
        }
        add("uio_mapping", uio_ok ? 0 : 1, "errors", uio_ok, detail, "os", "", false);
        add("write_combined_mapping", wc_ok ? 0 : 1, "errors", wc_ok, detail, "os", "", false);
        add("read_only_mapping", readonly ? 1 : 0, "bool", readonly, "status must be mapped r--s",
            "os", "", false);
        std::ifstream iomem("/proc/iomem");
        const bool reserved = hw::outside_system_ram(iomem, mem::buffers_addr, mem::buffers_range);
        const std::string reservation =
            "/sys/firmware/devicetree/base/reserved-memory/dma-buffer@18000000";
        const auto tree = read_file(reservation + "/reg");
        uint64_t reserved_base = 0, reserved_size = 0;
        if (tree.size() == 8)
        {
            for (unsigned i = 0; i < 4; ++i)
            {
                reserved_base = (reserved_base << 8) | static_cast<unsigned char>(tree[i]);
                reserved_size = (reserved_size << 8) | static_cast<unsigned char>(tree[i + 4]);
            }
        }
        std::error_code tree_error;
        const bool boot_reservation =
            std::filesystem::exists(reservation + "/no-map", tree_error) && tree.size() == 8 &&
            mem::buffers_addr >= reserved_base &&
            mem::buffers_addr + mem::buffers_range <= reserved_base + reserved_size;
        // /proc/iomem proves exclusion; the boot DT's no-map reservation is checked
        // by the host too.
        add("dma_reservation", reserved ? 1 : 0, "bool", reserved && boot_reservation,
            "boot no-map reservation and fixed 8 MiB buffer must not intersect "
            "System RAM",
            "os",
            "\"iomem\":" + quote(read_file("/proc/iomem")) +
                ",\"reservation_reg_bytes\":" + std::to_string(tree.size()),
            false);
        const bool first_snapshot = snapshot();
        const uint64_t cycle_start =
            sts.read<reg::cycles0>() | (uint64_t(sts.read<reg::cycles1>()) << 32);
        const auto clock_start = Clock::now();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        const bool second_snapshot = snapshot();
        const uint64_t cycle_end =
            sts.read<reg::cycles0>() | (uint64_t(sts.read<reg::cycles1>()) << 32);
        const double measured_clock = double(cycle_end - cycle_start) / seconds(clock_start);
        add("converter_clock", first_snapshot && second_snapshot ? measured_clock : unavailable,
            "Hz",
            first_snapshot && second_snapshot && measured_clock > 123750000 &&
                measured_clock < 126250000,
            "PL cycle counter against onboard monotonic clock, expected 125 MHz "
            "within 1%",
            "instrument", "", false);
        const auto before = resource_count("/proc/self/fd");
        bool lifetime = true;
        for (unsigned i = 0; i < 16; ++i)
        {
            Uio<mem::uart_a> temporary;
            lifetime &= temporary.open() >= 0 && temporary.mmap() != nullptr;
        }
        const auto after = resource_count("/proc/self/fd");
        add("mapping_lifetime", double(after) - double(before), "fds", lifetime && after == before,
            "16 UIO open/map/unmap/close cycles must retain no descriptors", "os", "", false);
    }
    void registers()
    {
        constexpr unsigned n = 20000;
        uint64_t errors = 0;
        unsigned written = 0, read = 0, write_passes = 0, read_passes = 0;
        auto start = Clock::now();
        for (unsigned i = 0; i < n && !cancelled; ++i)
        {
            ctl.write<reg::scratch>(pattern(i, 0x1234));
            ++written;
        }
        const double writes = seconds(start);
        start = Clock::now();
        uint32_t last = 0;
        for (unsigned i = 0; i < n && !cancelled; ++i)
        {
            last = sts.read<reg::scratch_echo>();
            ++read;
        }
        const double reads = seconds(start);
        if (written == 0 || last != pattern(written - 1, 0x1234))
            ++errors;
        for (unsigned i = 0; i < 256 && !cancelled; ++i)
        {
            const uint32_t expected = pattern(i, 0xabcdef);
            ctl.write<reg::scratch>(expected);
            barrier();
            if (ctl.read<reg::scratch>() != expected || sts.read<reg::scratch_echo>() != expected)
                ++errors;
        }
        add("register_read", read ? reads * 1e9 / read : unavailable, "ns/read", errors == 0,
            "20000 status reads; 256 exact write/read/echo cases");
        add("register_write", written ? writes * 1e9 / written : unavailable, "ns/write",
            errors == 0, "20000 config writes", "os",
            "\"integrity_errors\":" + std::to_string(errors) + ",\"completed_reads\":" +
                std::to_string(read) + ",\"completed_writes\":" + std::to_string(written));
        constexpr unsigned loops = 256, words = 1024;
        start = Clock::now();
        for (unsigned j = 0; j < loops && !cancelled; ++j)
        {
            for (unsigned i = 0; i < words; ++i)
                bram.write_reg(i * 4, pattern(i, j));
            ++write_passes;
        }
        barrier();
        const double bw = seconds(start);
        start = Clock::now();
        for (unsigned j = 0; j < loops && !cancelled; ++j)
        {
            for (unsigned i = 0; i < words; ++i)
                errors += bram.read_reg(i * 4) != pattern(i, write_passes - 1);
            ++read_passes;
        }
        const double br = seconds(start);
        add("bram_write", double(write_passes * words * 4) / bw / 1e6, "MB/s", errors == 0,
            "4 KiB bulk window, 256 passes, WC drain included");
        add("bram_read", double(read_passes * words * 4) / br / 1e6, "MB/s", errors == 0,
            "full data comparison on every pass", "os",
            "\"integrity_errors\":" + std::to_string(errors) +
                ",\"completed_read_passes\":" + std::to_string(read_passes) +
                ",\"completed_write_passes\":" + std::to_string(write_passes));
    }
    void dma_test(const std::string& workload, double duration, CombinedLoad* load = nullptr)
    {
        uint64_t errors = 0, transferred = 0;
        unsigned interrupts = 0, iterations = 0;
        double transfer_seconds = 0;
        std::vector<double> latencies;
        uint64_t window_bytes = 0;
        if (load)
        {
            load->dma_affinity_error = pin_worker(0);
            bool warm = dma.reset();
            dma.fill(65536, 0xabc);
            warm = dma.transfer(65536, true, true).ok && dma.verify(65536, 0xabc) == 0 && warm;
            warm = dma.reset() && warm;
            dma.fill(Dma::bytes, 0xb1570000U);
            if (!warm)
            {
                ++errors;
                load->ready.drop();
                load->ready.await_release();
            }
            else
                load->ready.wait();
            load->dma_start_us = seconds(load->ready.start()) * 1e6;
        }
        const auto start = load ? load->ready.start() : Clock::now();
        do
        {
            if ((!load || iterations != 0) && !dma.reset())
            {
                ++errors;
                break;
            }
            const uint32_t seed = 0xb1570000U + iterations;
            if (!load || iterations != 0)
                dma.fill(Dma::bytes, seed);
            const auto result = dma.transfer(Dma::bytes, true, true);
            errors += result.errors + (!result.ok);
            interrupts += result.interrupts;
            transfer_seconds += result.seconds;
            latencies.push_back(result.seconds * 1e3);
            errors += dma.verify(Dma::bytes, seed);
            transferred += result.ok ? Dma::bytes : 0;
            if (load && Clock::now() <= load->window_end() && result.ok)
                window_bytes += Dma::bytes;
            ++iterations;
            if (!result.ok)
                break;
        } while (!cancelled && (load ? !load->finished() : seconds(start) < duration));
        if (!snapshot())
            ++errors;
        const uint64_t tx =
            sts.read<reg::tx_words0>() | (uint64_t(sts.read<reg::tx_words1>()) << 32);
        const uint64_t rx =
            sts.read<reg::rx_words0>() | (uint64_t(sts.read<reg::rx_words1>()) << 32);
        errors += sts.read<reg::discontinuities>() + sts.read<reg::overflow>();
        if (tx != Dma::bytes / 4 || rx != Dma::bytes / 4 ||
            sts.read<reg::packets>() != Dma::bytes / Dma::chunk)
            ++errors;
        add("dma_" + workload,
            load                   ? double(window_bytes) / duration / 1e6
            : transfer_seconds > 0 ? double(transferred) / transfer_seconds / 1e6
                                   : unavailable,
            "MB/s", errors == 0 && iterations >= 2,
            "2 MiB full duplex SG, 32 x 64 KiB descriptors, exact pattern and "
            "descriptor lengths; "
            "buffers reused",
            "os",
            "\"errors\":" + std::to_string(errors) + ",\"interrupts\":" +
                std::to_string(interrupts) + ",\"iterations\":" + std::to_string(iterations) +
                ",\"bytes\":" + std::to_string(transferred) +
                ",\"window_bytes\":" + std::to_string(window_bytes) + ",\"engine_MB_s\":" +
                number(transfer_seconds > 0 ? double(transferred) / transfer_seconds / 1e6
                                            : unavailable) +
                ",\"latency_ms\":" + distribution(latencies) +
                ",\"tx_words\":" + std::to_string(tx) + ",\"rx_words\":" + std::to_string(rx) +
                ",\"fifo_overflow\":" + std::to_string(sts.read<reg::overflow>()));
        dma.stop();
    }
    std::array<SerialResult, 2> serial_exchange(size_t bytes, bool roundtrip,
                                                CombinedLoad* load = nullptr)
    {
        SerialEndpoint<mem::uart_a> a(cancelled);
        SerialEndpoint<mem::uart_b> b(cancelled);
        std::array<SerialResult, 2> result;
        if (!a.open() || !b.open())
        {
            result[0].errors = 1;
            if (load)
            {
                load->ready.drop();
                load->ready.drop();
            }
            return result;
        }
        if (!a.idle_timeout() || !b.idle_timeout())
        {
            result[0].errors = 1;
            if (load)
            {
                load->ready.drop();
                load->ready.drop();
            }
            return result;
        }
        a.reset();
        b.reset();
        StartGate ready(2);
        StartGate* gate = load ? &load->ready : &ready;
        std::thread ta([&] { result[0] = a.exchange(bytes, 0, roundtrip, gate, load); });
        std::thread tb([&] { result[1] = b.exchange(bytes, 1, roundtrip, gate, load); });
        ta.join();
        tb.join();
        return result;
    }
    void serial_test(const std::string& workload, CombinedLoad* load = nullptr)
    {
        auto result = serial_exchange(profile.uart_bytes(), false, load);
        uint64_t errors = 0;
        double elapsed = 0;
        size_t bytes = 0;
        unsigned interrupts = 0;
        LatencyHistogram observation;
        size_t window_bytes = 0;
        std::ostringstream endpoints;
        endpoints << '[';
        for (unsigned i = 0; i < 2; ++i)
        {
            auto& r = result[i];
            errors += r.errors;
            elapsed = std::max(elapsed, r.seconds);
            bytes += r.received;
            interrupts += r.interrupts;
            observation.merge(r.observation_us);
            window_bytes += r.window_received;
            if (i)
                endpoints << ',';
            endpoints << "{\"channel\":" << i << ",\"received\":" << r.received
                      << ",\"transmitted\":" << r.transmitted << ",\"errors\":" << r.errors
                      << ",\"interrupts\":" << r.interrupts << ",\"poll_timeouts\":" << r.timeouts
                      << ",\"scheduling_error\":" << r.scheduling_error
                      << ",\"frame_cycles\":" << r.frame_cycles
                      << ",\"observation_us\":" << r.observation_us.json()
                      << ",\"window_observation_us\":" << r.window_observation_us.json()
                      << ",\"window_received\":" << r.window_received
                      << ",\"start_offset_us\":" << number(r.start_offset_us)
                      << ",\"diagnostics\":[";
            for (size_t j = 0; j < r.diagnostics.size(); ++j)
            {
                if (j)
                    endpoints << ',';
                endpoints << quote(r.diagnostics[j]);
            }
            endpoints << "]}";
        }
        endpoints << ']';
        add("uart_" + workload,
            load          ? double(window_bytes) / load->window_seconds / 1000
            : elapsed > 0 ? double(bytes) / elapsed / 1000
                          : unavailable,
            "kB/s aggregate", errors == 0 && interrupts > 4,
            "deterministic bytes each way at 460800 baud 8N1; sequence checked "
            "byte for byte",
            "os",
            "\"channels\":" + endpoints.str() + ",\"errors\":" + std::to_string(errors) +
                ",\"full_elapsed_seconds\":" + number(elapsed) +
                ",\"window_bytes\":" + std::to_string(window_bytes));
        {
            const auto exchange = serial_exchange(32 * 24, true);
            auto rtt = exchange[0].roundtrip_us;
            const uint64_t roundtrip_errors = exchange[0].errors + exchange[1].errors;
            std::sort(rtt.begin(), rtt.end());
            add("uart_roundtrip_" + workload, rtt.empty() ? 0 : rtt[rtt.size() / 2], "us",
                roundtrip_errors == 0 && rtt.size() == 24,
                "24 request/reply frames, 32 bytes each, persistent UIO workers; "
                "idle IRQ timeout "
                "before traffic",
                "os", "\"latency_us\":" + distribution(rtt), workload == "isolated");
        }
        if (workload == "isolated")
        {
            const bool baud_ok = result[0].frame_cycles >= 2712 && result[0].frame_cycles <= 2713 &&
                                 result[1].frame_cycles >= 2712 && result[1].frame_cycles <= 2713;
            add("uart_baud", result[0].frame_cycles ? 1250000000.0 / result[0].frame_cycles : 0,
                "baud", baud_ok,
                "independent PL frame counter: ten bit-times must take 2712 or 2713 "
                "ADC-clock "
                "cycles");
            add("uio_observation", observation.percentile(.5), "us", observation.count > 0,
                "PL IRQ assertion to userspace clock-register read; includes AXI "
                "read overhead",
                "os", "\"latency_us\":" + observation.json());
        }
    }
    void rpc_test(const std::string& workload, bool bulk = false, CombinedLoad* load = nullptr)
    {
        const auto result = rpc_benchmark(bulk, load ? load->window_seconds : profile.rpc_seconds(),
                                          cancelled, load);
        add("rpc_" + (bulk ? std::string("bulk") : workload),
            result.elapsed > 0
                ? (bulk ? double(result.bytes) / 1e6 : double(result.calls)) / result.elapsed
                : 0,
            bulk ? "MB/s" : "requests/s", result.errors == 0 && result.calls > 0,
            bulk ? "onboard TCP RPC: 1024-word generated vector reply, exact "
                   "payload checked"
                 : "onboard TCP RPC: generated ping decode, dispatch and reply; "
                   "one request "
                   "outstanding",
            "os",
            "\"errors\":" + std::to_string(result.errors) + ",\"calls\":" +
                std::to_string(result.calls) + ",\"latency_us\":" + result.us.json() +
                ",\"late_calls\":" + std::to_string(result.late_calls) +
                ",\"diagnostic\":" + quote(result.diagnostic));
    }
    void analog(unsigned mask)
    {
        if (!dma.reset())
        {
            add("waveform_upload", 0, "bool", false, "DMA reset timed out", "instrument", "",
                false);
            return;
        }
        for (unsigned i = 0; i < 1024; ++i)
        {
            const int code =
                static_cast<int>(std::lround(4096 * std::sin(2 * std::numbers::pi * i / 32)));
            const uint32_t word = uint32_t(code) & 0x3fffU;
            ram.write_reg(Dma::tx_data + 4 * i, word | (word << 16));
        }
        barrier();
        ctl.write<reg::mode>(1);
        auto upload = dma.transfer(4096, true, false);
        uint64_t errors = 0;
        for (unsigned i = 0; i < 1024; ++i)
            errors += bram.read_reg(i * 4) != ram.read_reg(Dma::tx_data + i * 4);
        add("waveform_upload", double(errors), "errors", upload.ok && errors == 0,
            "SG DMA upload into the CPU-accessible BRAM must match all 1024 packed "
            "DAC words",
            "instrument", "", false);
        ctl.write<reg::mode>(2);
        ctl.write<reg::samples>(16384);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        const auto capture = dma.transfer(65536, false, true, true);
        const bool counters_ok = snapshot();
        const bool capture_ok = capture.ok && counters_ok && sts.read<reg::overflow>() == 0;
        add("adc_capture", capture_ok ? 16384 : unavailable, "samples/channel", capture_ok,
            "packed ADC DMA, descriptor lengths, snapshot acknowledgement and no "
            "FIFO loss; "
            "independent of cables",
            "instrument",
            "\"dma_ok\":" + std::string(capture.ok ? "true" : "false") +
                ",\"snapshot_ok\":" + (counters_ok ? "true" : "false"),
            false);
        for (unsigned channel = 0; channel < 2; ++channel)
        {
            std::vector<int16_t> data(16384);
            for (unsigned i = 0; i < data.size(); ++i)
                data[i] = adc_signed(ram.read_reg(Dma::rx_data + i * 4), channel);
            const auto tone = fit_tone(data);
            const bool connected = (mask & (1U << channel)) != 0;
            const double snr =
                tone.residual_rms > 0
                    ? 20 * std::log10(tone.amplitude / std::sqrt(2.0) / tone.residual_rms)
                    : 0;
            const bool healthy = capture_ok && tone.amplitude > 2500 && tone.amplitude < 6000 &&
                                 snr > 30 && tone.clipping < 1 && sts.read<reg::overflow>() == 0;
            add("analog_" + std::to_string(channel), capture_ok ? tone.amplitude : unavailable,
                "ADC codes peak", connected ? healthy : capture_ok,
                connected ? "DAC to ADC cable; coherent 3.90625 MHz tone, 15.625 "
                            "MS/s capture; "
                            "gain is uncalibrated"
                          : "external cable absent; analog validation skipped, input "
                            "noise still "
                            "measured",
                "instrument",
                "\"mean_codes\":" + number(capture_ok ? tone.mean : unavailable) +
                    ",\"residual_rms_codes\":" +
                    number(capture_ok ? tone.residual_rms : unavailable) +
                    ",\"snr_db\":" + number(capture_ok ? snr : unavailable) +
                    ",\"clip_count\":" + number(capture_ok ? tone.clipping : unavailable) +
                    ",\"dma_ok\":" + (capture.ok ? "true" : "false") +
                    ",\"fifo_overflow\":" + std::to_string(sts.read<reg::overflow>()),
                false, capture_ok && !connected ? "skipped" : "");
            if (connected && !healthy)
            {
                std::ostringstream raw;
                for (unsigned i = 0; i < 256; ++i)
                    raw << data[i] << '\n';
                std::lock_guard lock(mutex);
                write_file(prefix + "/failure-adc" + std::to_string(channel) + ".txt", raw.str());
            }
        }
        dma.stop();
    }
    bool recovered_dma()
    {
        if (!dma.reset())
            return false;
        dma.fill(65536, 0xabc);
        return dma.transfer(65536, true, true).ok && dma.verify(65536, 0xabc) == 0;
    }
    void diagnostics()
    {
        log("Diagnostic profile: bounded fault detection followed by verified "
            "recovery.");
        bool good = recovered_dma();
        ram.write_reg(Dma::rx_data + 128, ram.read_reg(Dma::rx_data + 128) ^ 1U);
        barrier();
        const bool detected = dma.verify(65536, 0xabc) == 1;
        const bool recovered = recovered_dma();
        add("fault_corrupt_payload", detected ? 1 : 0, "bool", good && detected && recovered,
            "flip one completed RX word; detect exact mismatch; reset and verify "
            "fresh transfer",
            "instrument", "", false);
        for (const auto [fault, name] :
             std::array{std::pair{DmaFault::bad_descriptor, "fault_bad_descriptor"},
                        std::pair{DmaFault::stalled_stream, "fault_stalled_stream"},
                        std::pair{DmaFault::missing_irq, "fault_missing_irq"}})
        {
            if (cancelled)
                break;
            good = dma.reset();
            dma.fill(65536, 0xdef);
            const auto result = dma.transfer(65536, true, true, false, fault, 150);
            const bool hardware_error = ((result.tx_status | result.rx_status) & 0x770U) != 0;
            const bool detected_fault =
                !result.ok && (fault != DmaFault::bad_descriptor || hardware_error);
            const bool recovered_fault = recovered_dma();
            add(name, result.seconds, "s",
                good && detected_fault && recovered_fault && result.seconds < .25,
                "injected fault must fail within 150 ms plus IRQ wait; reset must "
                "recover exact "
                "DMA data",
                "instrument",
                "\"tx_status\":" + std::to_string(result.tx_status) +
                    ",\"rx_status\":" + std::to_string(result.rx_status) +
                    ",\"interrupts\":" + std::to_string(result.interrupts) +
                    ",\"recovered\":" + (recovered_fault ? "true" : "false"),
                false);
        }
        ctl.write<reg::mode>(256);
        barrier();
        const bool timed_out = !snapshot();
        ctl.write<reg::mode>(0);
        barrier();
        const bool snapshot_recovered = snapshot();
        add("fault_snapshot_ack", timed_out ? 1 : 0, "bool", timed_out && snapshot_recovered,
            "hold path in reset; require snapshot timeout; release reset and "
            "require new "
            "acknowledgement",
            "instrument", "", false);
    }
    void controlled()
    {
        log("Controlled RPC comparison: fixed offered load, no UART, no report "
            "writes in windows.");
        const bool dma_ready = (comparison_mask & 12) == 0 || dma.open();
        // Let an already-open webpage discover comparison_active before
        // measurement.
        for (unsigned i = 0; i < 150 && !cancelled; ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        for (unsigned index = 0; index < comparison_cases.size() && !cancelled; ++index)
        {
            if (!(comparison_mask & (1U << index)))
                continue;
            const bool memory_on = index == 1 || index == 3, dma_on = index == 2 || index == 3;
            std::vector<uint64_t> memory(memory_on ? 1024 * 1024 : 0, 0x42495354);
            PacedResult rpc, cpu, transfer;
            uint64_t checksum = 0;
            uint32_t seed = 0;
            StartGate gate(3);
            {
                std::lock_guard lock(mutex);
                defer_io = true;
            }
            observer_requests = 0;
            measuring = true;
            std::thread cpu_worker(
                [&]
                {
                    cpu.affinity_error = pin_worker(1);
                    size_t offset = 0;
                    gate.wait();
                    if (memory_on)
                        paced(cpu, gate.start(), comparison_memory_rate, comparison_seconds,
                              cancelled,
                              [&]
                              {
                                  constexpr size_t words =
                                      comparison_memory_bytes / sizeof(uint64_t);
                                  for (size_t i = 0; i < words; ++i)
                                  {
                                      auto& word = memory[offset + i];
                                      word = (word + checksum) * 1664525U + 1013904223U;
                                      checksum ^= word;
                                  }
                                  offset = (offset + words) % memory.size();
                                  return true;
                              });
                });
            std::thread rpc_worker([&] { rpc = controlled_rpc(gate, cancelled, index); });
            transfer.affinity_error = pin_worker(0);
            const bool ready = !dma_on || (dma_ready && dma.reset());
            if (!ready)
            {
                transfer.errors = 1;
                gate.drop();
            }
            else
            {
                gate.wait();
                if (dma_on)
                    paced(transfer, gate.start(), comparison_dma_rate, comparison_seconds,
                          cancelled,
                          [&]
                          {
                              if (!dma.reset())
                                  return false;
                              dma.fill(comparison_dma_bytes, ++seed);
                              const auto result = dma.transfer(comparison_dma_bytes, true, true,
                                                               false, DmaFault::none, 100);
                              return result.ok && dma.verify(comparison_dma_bytes, seed) == 0;
                          });
            }
            rpc_worker.join();
            cpu_worker.join();
            measuring = false;
            const auto observers = observer_requests.load();
            const bool integrity = !cancelled && rpc.errors == 0 && transfer.errors == 0 &&
                                   cpu.affinity_error == 0 && rpc.affinity_error == 0 &&
                                   transfer.affinity_error == 0 && rpc.completed != 0;
            const bool valid = integrity && rpc.valid() && (!memory_on || cpu.valid()) &&
                               (!dma_on || transfer.valid()) && observers == 0 &&
                               gate.failures() == 0;
            {
                std::lock_guard lock(mutex);
                defer_io = false;
            }
            add("rpc_controlled_" + std::string(comparison_cases[index]),
                rpc.latency.percentile(.5), "us", integrity,
                "client wall RTT includes dispatch, reply and scheduling; valid load "
                "requires "
                "98% completed slots and zero observer requests",
                "os",
                "\"comparison_quality\":" + quote(valid ? "valid" : "inconclusive") +
                    ",\"observer_requests\":" + std::to_string(observers) + ",\"rpc\":" +
                    rpc.json() + ",\"memory\":" + cpu.json() + ",\"dma\":" + transfer.json() +
                    ",\"memory_checksum\":" + quote(std::to_string(checksum)));
        }
    }
    void run(unsigned mask)
    {
        if (profile.id == 3)
        {
            controlled();
            finish();
            return;
        }
        // The previous run and shutdown leave the shared path in reset.
        ctl.write<reg::mode>(0);
        barrier();
        log("Validation started. Fixed workload v3: bounded waits; timed regions exclude report "
            "writes.");
        mappings();
        if (!cancelled)
            registers();
        const bool ready = dma.open();
        add("dma_uio_interrupt_binding", ready ? 1 : 0, "bool", ready,
            "DMA must expose UIO interrupt and register map", "os", "", false);
        if (ready && !cancelled)
            dma_test("isolated", profile.dma_seconds());
        if (!cancelled)
            serial_test("isolated");
        if (!cancelled)
            rpc_test("isolated");
        if (!cancelled)
            rpc_test("isolated", true);
        if (ready && !cancelled)
            analog(mask);
        if (ready && !cancelled)
        {
            log("Combined CPU/memory + SG DMA + duplex UART + generated RPC workload started.");
            {
                std::lock_guard lock(mutex);
                defer_io = true;
            }
            rusage before{}, after{};
            getrusage(RUSAGE_SELF, &before);
            CombinedLoad load;
            load.window_seconds = profile.window_seconds();
            observer_requests = 0;
            std::atomic<uint64_t> burn_bytes{0}, burn_checksum{0};
            std::thread burn(
                [&]
                {
                    std::vector<uint64_t> memory(1024 * 1024, 0x42495354);
                    load.cpu_affinity_error = pin_worker(1);
                    uint64_t accumulator = 0;
                    load.ready.wait();
                    load.cpu_start_us = seconds(load.ready.start()) * 1e6;
                    while (!load.finished() && !cancelled)
                    {
                        for (auto& word : memory)
                        {
                            word = (word + accumulator) * 1664525U + 1013904223U;
                            accumulator ^= word;
                        }
                        if (Clock::now() <= load.window_end())
                            burn_bytes.fetch_add(memory.size() * 16, std::memory_order_relaxed);
                    }
                    burn_checksum = accumulator;
                    barrier();
                });
            std::thread serial(
                [&]
                {
                    serial_test("combined", &load);
                    load.serial_done = true;
                });
            std::thread rpc(
                [&]
                {
                    rpc_test("combined", false, &load);
                    load.rpc_done = true;
                });
            dma_test("combined", load.window_seconds, &load);
            serial.join();
            rpc.join();
            burn.join();
            const double elapsed = seconds(load.ready.start());
            const bool window_ok =
                load.ready.failures() == 0 && !cancelled && load.cpu_affinity_error == 0 &&
                load.dma_affinity_error == 0 && load.rpc_affinity_error == 0 &&
                std::max({load.cpu_start_us, load.dma_start_us, load.rpc_start_us}) < 20000;
            add("combined_window", load.window_seconds, "s", window_ok,
                "shared epoch after warmup; CPU/DMA/RPC loads persist through UART verification; "
                "start skew below 20 ms",
                "instrument",
                "\"dropped_participants\":" + std::to_string(load.ready.failures()) +
                    ",\"cpu_start_us\":" + number(load.cpu_start_us) + ",\"dma_start_us\":" +
                    number(load.dma_start_us) + ",\"rpc_start_us\":" + number(load.rpc_start_us) +
                    ",\"observer_requests\":" + std::to_string(observer_requests) +
                    ",\"drain_seconds\":" + number(std::max(0.0, elapsed - load.window_seconds)),
                false);
            getrusage(RUSAGE_SELF, &after);
            {
                std::lock_guard lock(mutex);
                defer_io = false;
                persist();
            }
            const double cpu = (after.ru_utime.tv_sec - before.ru_utime.tv_sec) +
                               (after.ru_utime.tv_usec - before.ru_utime.tv_usec) / 1e6 +
                               (after.ru_stime.tv_sec - before.ru_stime.tv_sec) +
                               (after.ru_stime.tv_usec - before.ru_stime.tv_usec) / 1e6;
            add("cpu_memory_load", double(burn_bytes) / load.window_seconds / 1e6,
                "MB/s read+write", burn_bytes > 0,
                "one CPU/memory worker over 8 MiB during concurrent DMA/UART/RPC; server RSS and "
                "scheduling recorded",
                "os",
                "\"checksum\":" + quote(std::to_string(burn_checksum)) +
                    ",\"process_cpu_seconds\":" + number(cpu) + ",\"elapsed_seconds\":" +
                    number(elapsed) + ",\"max_rss_kib\":" + std::to_string(after.ru_maxrss) +
                    ",\"major_faults\":" + std::to_string(after.ru_majflt - before.ru_majflt) +
                    ",\"involuntary_switches\":" +
                    std::to_string(after.ru_nivcsw - before.ru_nivcsw) +
                    ",\"meminfo\":" + quote(read_file("/proc/meminfo")));
        }
        if (profile.id == 2 && ready && !cancelled)
            diagnostics();
        finish();
    }
    void finish()
    {
        dma.stop();
        add("suite_duration", seconds(beginning), "s",
            seconds(beginning) < (profile.id == 1 ? 60 : 30) && !cancelled,
            "onboard and client budgets: health/diagnostic 30 s, explicit benchmark 60 s", "os", "",
            false);
        {
            std::lock_guard lock(mutex);
            state = cancelled ? "cancelled" : "complete";
            if (cancelled)
                functional = "incomplete";
            persist();
        }
        active = false;
    }
};

Bist::Bist() : impl(std::make_unique<Impl>()) {}
Bist::~Bist() = default;
bool Bist::start(uint32_t analog_mask) { return start_profile(analog_mask, 0); }
bool Bist::start_profile(uint32_t analog_mask, uint32_t profile)
{
    if (profile == 4)
        return start_comparison(48);
    return start_impl(analog_mask, profile, 15);
}
bool Bist::start_comparison(uint32_t case_mask) { return start_impl(0, 3, case_mask); }
bool Bist::comparison_active()
{
    if (impl->measuring)
        ++impl->observer_requests;
    return impl->active && impl->profile.id == 3;
}
bool Bist::start_impl(uint32_t analog_mask, uint32_t profile, uint32_t case_mask)
{
    if (analog_mask > 3 || profile > 3 || case_mask == 0 || case_mask > 63 || impl->active)
        return false;
    if (impl->worker.joinable())
        impl->worker.join();
    std::error_code error;
    std::filesystem::create_directories(root, error);
    if (error)
        return false;
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    std::lock_guard lock(impl->mutex);
    impl->run_id =
        std::to_string(std::chrono::duration_cast<std::chrono::microseconds>(now).count());
    impl->prefix = std::string(root) + "/" + impl->run_id;
    std::filesystem::create_directory(impl->prefix, error);
    if (error)
        return false;
    impl->profile.id = profile;
    impl->comparison_mask = case_mask;
    const unsigned version_id = profile == 3 ? ((case_mask & 48) ? 5 : 4) : workload_version;
    impl->functional = "pass";
    impl->performance_status = "not_evaluated";
    impl->performance.clear();
    impl->scorecard.clear();
    impl->storage_ok = true;
    impl->metrics = "[]";
    impl->values.clear();
    impl->log_text.clear();
    impl->state = "running";
    utsname system{};
    uname(&system);
    auto version = read_file("/tmp/live-instrument/version");
    version.erase(version.find_last_not_of("\r\n") + 1);
    if (version.empty())
        version = "unavailable";
    const auto os_release = read_file("/etc/os-release");
    const auto os = release_fields(os_release);
    const auto image = release_fields(read_file("/etc/koheron-release"));
    const auto manifest = release_fields(read_file("/usr/local/share/koheron/manifest.txt"));
    const auto fpga_digest = digest("/tmp/live-instrument/bist.bit.bin");
    const auto server_digest = digest("/proc/" + std::to_string(getpid()) + "/exe");
    std::ostringstream summary;
    summary << "Instrument: " << INSTRUMENT_NAME << ' ' << version << '\n'
            << "SDK: " << KOHERON_VERSION << '\n'
            << "OS: " << field(os, "PRETTY_NAME") << '\n'
            << "Kernel: " << system.release << '\n'
            << "OS image: " << field(image, "RELEASE") << " | build " << field(image, "BUILD_ID")
            << '\n'
            << "OS source: " << field(image, "GIT_DESCRIBE") << '\n'
            << "Board: STEMlab 125-14 Gen1\n"
            << "FPGA SHA256: " << fpga_digest << '\n'
            << "Server SHA256: " << server_digest << '\n'
            << "BIST source SHA256: " << BIST_BUILD_ID << '\n'
            << "Workload: " << version_id << " | analog cable mask: " << analog_mask << '\n';
    impl->metadata_text = summary.str();
    const auto parameters = impl->profile;
    impl->metadata =
        "{\"bist_version\":" + quote(version) + ",\"instrument_name\":" + quote(INSTRUMENT_NAME) +
        ",\"instrument_version\":" + quote(version) +
        ",\"workload_version\":" + std::to_string(version_id) +
        ",\"sdk_version\":" + quote(KOHERON_VERSION) +
        ",\"bist_build_id\":" + quote(BIST_BUILD_ID) + ",\"started_utc\":" + quote(timestamp()) +
        ",\"kernel\":" + quote(system.release) + ",\"os_release\":" + quote(os_release) +
        ",\"os\":" + fields_json(os) + ",\"os_image\":" + fields_json(image) +
        ",\"os_manifest\":" + fields_json(manifest) +
        ",\"cpuinfo\":" + quote(read_file("/proc/cpuinfo")) +
        ",\"kernel_cmdline\":" + quote(read_file("/proc/cmdline")) +
        ",\"boot_id\":" + quote(read_file("/proc/sys/kernel/random/boot_id")) +
        ",\"board\":\"STEMlab 125-14 Gen1\",\"fpga_identity\":" +
        std::to_string(impl->sts.read<reg::identity>()) + ",\"fpga_dna\":" +
        quote(std::to_string(impl->sts.read<reg::dna0>() |
                             (uint64_t(impl->sts.read<reg::dna1>()) << 32))) +
        ",\"fpga_sha256\":" + quote(fpga_digest) + ",\"server_sha256\":" + quote(server_digest) +
        ",\"instrument_manifest\":" + quote(read_file("/tmp/live-instrument/instrument.json")) +
        ",\"environment\":{\"interrupts\":" + quote(read_file("/proc/interrupts")) +
        ",\"cpu0_governor\":" +
        quote(read_file("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor")) +
        ",\"temperature_millic\":" + quote(read_file("/sys/class/thermal/thermal_zone0/temp")) +
        ",\"cpu0_frequency_khz\":" +
        quote(read_file("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq")) +
        "},\"parameters\":{\"analog_mask\":" + std::to_string(analog_mask) +
        ",\"adc_clock_hz\":125000000,\"capture_decimation\":8,\"baud\":460800,"
        "\"framing\":\"8N1\","
        "\"dma_bytes\":2097152,\"dma_descriptors\":32,\"dma_chunk_bytes\":65536,"
        "\"uart_bytes_per_"
        "direction\":" +
        std::to_string(parameters.uart_bytes()) +
        ",\"uart_fifo_bytes\":64,\"uart_scheduler\":\"SCHED_FIFO\",\"uart_"
        "priority\":1,\"uart_service_pause_us\":100,\"uart_worker_cpus\":[0,1],"
        "\"dma_min_"
        "iterations\":2,"
        "\"rpc_io_timeout_seconds\":1,\"dma_isolated_seconds\":" +
        number(parameters.dma_seconds()) +
        ",\"rpc_isolated_seconds\":" + number(parameters.rpc_seconds()) +
        ",\"combined_window_seconds\":" + number(parameters.window_seconds()) +
        ",\"profile\":" + quote(parameters.name()) +
        ",\"load_worker_cpus\":{\"cpu\":1,\"dma\":0,\"rpc_client\":0},"
        "\"percentile_min_samples\":{"
        "\"p95\":100,\"p99\":1000},\"statistics\":\"whole_population_histogram_"
        "v1\"" +
        (profile == 3 ? ",\"controlled\":" + comparison_parameters(case_mask) : "") +
        "},\"comparability\":\"Require matching FPGA and "
        "server "
        "SHA256, BIST source identity, workload version, "
        "parameters and wiring; absent analog cables are skipped\"}";
    impl->write_file(std::string(root) + "/latest", impl->run_id);
    impl->persist();
    impl->cancelled = false;
    impl->active = true;
    impl->beginning = Clock::now();
    impl->worker = std::thread([this, analog_mask] { impl->run(analog_mask); });
    return true;
}
void Bist::cancel() { impl->cancelled = true; }
bool Bist::running()
{
    if (impl->measuring)
        ++impl->observer_requests;
    return impl->active;
}
double Bist::get_boot_seconds()
{
    timespec now{};
    clock_gettime(CLOCK_BOOTTIME, &now);
    return double(now.tv_sec) + double(now.tv_nsec) / 1e9;
}
std::string Bist::get_boot_id()
{
    auto id = read_file("/proc/sys/kernel/random/boot_id");
    id.erase(id.find_last_not_of("\r\n") + 1);
    return id;
}
std::string Bist::get_report()
{
    ++impl->observer_requests;
    std::lock_guard lock(impl->mutex);
    return impl->report_unlocked();
}
std::string Bist::get_scorecard()
{
    ++impl->observer_requests;
    std::lock_guard lock(impl->mutex);
    return impl->scorecard;
}
std::string Bist::get_log(uint32_t cursor)
{
    ++impl->observer_requests;
    std::lock_guard lock(impl->mutex);
    const size_t begin = std::min(size_t(cursor), impl->log_text.size()),
                 end = std::min(begin + 16384, impl->log_text.size());
    return "{\"run_id\":" + quote(impl->run_id) + ",\"cursor\":" + std::to_string(end) +
           ",\"state\":" + quote(impl->state) +
           ",\"text\":" + quote(std::string_view(impl->log_text).substr(begin, end - begin)) + "}";
}
uint32_t Bist::ping(uint32_t value) { return value ^ 0x42495354U; }
std::vector<uint32_t> Bist::benchmark_reply(uint32_t words)
{
    std::vector<uint32_t> result(std::min(words, 16384U));
    for (size_t i = 0; i < result.size(); ++i)
        result[i] = pattern(uint32_t(i), 0x42495354U);
    return result;
}
uint32_t Bist::benchmark_upload(std::vector<uint32_t> words, uint32_t seed)
{
    const auto ack = upload_ack(words, seed);
    if (ack != 0)
        impl->upload_storage = std::move(words);
    return ack;
}
uint32_t Bist::benchmark_upload_reference(const std::vector<uint32_t>& words, uint32_t seed)
{
    return upload_ack(words, seed);
}

bool Bist::record_startup(double network_seconds, double server_seconds, double boot_seconds)
{
    if (impl->active || impl->run_id.empty() || !std::isfinite(network_seconds) ||
        !std::isfinite(server_seconds) || !std::isfinite(boot_seconds) || network_seconds < 0 ||
        server_seconds < network_seconds || boot_seconds < 0)
        return false;
    impl->add("network_readiness", network_seconds, "s", true,
              "host monotonic time from reboot request to first new-boot ICMP "
              "echo reply");
    impl->add("server_readiness", server_seconds, "s", true,
              "host monotonic time from reboot request to generated BIST RPC "
              "readiness");
    impl->add("boot_readiness", boot_seconds, "s", boot_seconds < 60,
              "Linux monotonic uptime at first BIST RPC readiness");
    return impl->storage_ok;
}
bool Bist::record_lifecycle(double restart_seconds, bool recovered)
{
    if (impl->active || impl->run_id.empty() || !std::isfinite(restart_seconds) ||
        restart_seconds < 0)
        return false;
    impl->cancelled = false;
    bool dma_ok = impl->dma.open() && impl->dma.reset();
    Transfer transfer;
    if (dma_ok)
    {
        impl->dma.fill(65536, 0x1234);
        transfer = impl->dma.transfer(65536, true, true);
        dma_ok = transfer.ok && impl->dma.verify(65536, 0x1234) == 0;
    }
    auto serial = impl->serial_exchange(64, false);
    const bool healthy = dma_ok && serial[0].errors == 0 && serial[1].errors == 0;
    impl->dma.stop();
    impl->add("instrument_restart", restart_seconds, "s", recovered && healthy,
              "report retained across stop/start; overlay reapplied; DMA and "
              "both UARTs recover",
              "os",
              "\"report_retained\":" + std::string(recovered ? "true" : "false") +
                  ",\"dma_ok\":" + (dma_ok ? "true" : "false") +
                  ",\"dma_tx_status\":" + std::to_string(transfer.tx_status) +
                  ",\"dma_rx_status\":" + std::to_string(transfer.rx_status) +
                  ",\"uart_a_errors\":" + std::to_string(serial[0].errors) +
                  ",\"uart_b_errors\":" + std::to_string(serial[1].errors),
              false);
    return recovered && healthy && impl->storage_ok;
}

bool Bist::record_performance(std::string run_id, std::string status, std::string comparison_json)
{
    std::lock_guard lock(impl->mutex);
    if (impl->active || impl->state != "complete" || run_id != impl->run_id ||
        comparison_json.size() > 65536 ||
        (status != "pass" && status != "regression" && status != "inconclusive" &&
         status != "incompatible"))
        return false;
    impl->performance_status = std::move(status);
    impl->performance = std::move(comparison_json);
    impl->persist();
    return impl->storage_ok;
}
