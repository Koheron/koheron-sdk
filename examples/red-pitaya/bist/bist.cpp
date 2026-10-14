#include "bist.hpp"
#include "hardware.hpp"
#include "uart.hpp"
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

namespace
{
using namespace bist;
constexpr auto root = "/var/lib/koheron/bist";
struct Metric
{
    std::string name, domain, unit, detail, extra;
    double value;
    bool pass;
    bool headline;
    std::string json() const
    {
        return "{\"name\":" + quote(name) + ",\"domain\":" + quote(domain) +
               ",\"unit\":" + quote(unit) + ",\"value\":" + number(value) +
               ",\"pass\":" + (pass ? "true" : "false") +
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
        if (done <= 0)
            return false;
        p += done;
        size -= static_cast<size_t>(done);
    }
    return true;
}
struct RpcResult
{
    uint64_t calls = 0, errors = 0, bytes = 0;
    double elapsed = 0;
    std::vector<double> us;
};
RpcResult rpc_benchmark(bool bulk, double duration, std::atomic<bool>& cancel)
{
    RpcResult result;
    const int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0)
    {
        result.errors = 1;
        return result;
    }
    timeval timeout{0, 500000};
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
        ::close(fd);
        result.errors = 1;
        return result;
    }
    // Resolve IDs from the generated adapter's operation metadata.
    using Adapter = koheron::adapter_meta<koheron::BistAdapter>;
    uint32_t ping_id = 0, bulk_id = 0;
    [&]<size_t... I>(std::index_sequence<I...>)
    {
        ((
             [&]
             {
                 using Operation = std::tuple_element_t<I, typename Adapter::ops_tuple>;
                 const auto name = koheron::op_name<koheron::op_meta<Operation>::pmf>();
                 if (name == "ping")
                     ping_id = uint32_t(I);
                 if (name == "benchmark_reply")
                     bulk_id = uint32_t(I);
             }()),
         ...);
    }(std::make_index_sequence<std::tuple_size_v<typename Adapter::ops_tuple>>{});
    const uint32_t operation = (uint32_t(Adapter::id) << 16) | (bulk ? bulk_id : ping_id);
    std::array<uint32_t, 3> request{0, htonl(operation), htonl(bulk ? 1024U : 0x13579bdfU)};
    std::vector<uint32_t> response(bulk ? 1024 : 1);
    const auto start = Clock::now();
    while (!cancel && seconds(start) < duration)
    {
        const auto call_start = Clock::now();
        std::array<uint32_t, 3> header{};
        if (!socket_io(fd, request.data(), 12, true) ||
            !socket_io(fd, header.data(), bulk ? 12 : 8, false))
        {
            ++result.errors;
            break;
        }
        if (header[0] != 0 || ntohl(header[1]) != operation || (bulk && ntohl(header[2]) != 4096))
        {
            ++result.errors;
            break;
        }
        if (!socket_io(fd, response.data(), response.size() * 4, false))
        {
            ++result.errors;
            break;
        }
        if (bulk)
        {
            for (size_t i = 0; i < response.size(); ++i)
                result.errors += response[i] != pattern(uint32_t(i), 0x42495354U);
        }
        else
            result.errors += ntohl(response[0]) != (0x13579bdfU ^ 0x42495354U);
        ++result.calls;
        result.bytes += response.size() * 4;
        if (result.us.size() < 8192)
            result.us.push_back(seconds(call_start) * 1e6);
    }
    result.elapsed = seconds(start);
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
    std::string run_id, prefix, metrics = "[]", metadata = "{}", state = "idle", log_text;
    std::vector<Metric> values;
    bool storage_ok = true;
    bool defer_io = false;
    Clock::time_point beginning;
    Dma dma{dma_regs, ram, ctl, cancelled};

    Impl()
    {
        run_id = read_file(std::string(root) + "/latest");
        if (!run_id.empty() && run_id.find_first_not_of("0123456789-") == std::string::npos)
        {
            prefix = std::string(root) + "/" + run_id;
            metrics = read_file(prefix + "/metrics.json");
            metadata = read_file(prefix + "/metadata.json");
            state = read_file(prefix + "/state");
            log_text = read_file(prefix + "/log.txt");
            if (metrics.empty())
                metrics = "[]";
            if (metadata.empty())
                metadata = "{}";
            if (state == "running")
            {
                state = "interrupted";
                persist();
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
        constexpr std::array<std::string_view, 31> required{"uio_mapping",
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
        for (const auto name : required)
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
               ",\"metrics\":" + metrics + ",\"missing_measurements\":" + missing + "}";
    }
    void persist()
    {
        if (prefix.empty() || defer_io)
            return;
        write_file(prefix + "/metrics.json", metrics);
        write_file(prefix + "/metadata.json", metadata);
        write_file(prefix + "/state", state);
        write_file(prefix + "/log.txt", log_text);
        write_file(prefix + "/report.json", report_unlocked());
        std::ostringstream card;
        card << "Red Pitaya BIST 1.0.0 | run " << run_id << " | " << state << "\n"
             << "OS validation and reference instrument health are separate domains.\n";
        for (const auto& value : values)
            card << (value.pass ? "PASS " : "FAIL ") << value.domain << " | " << value.name << " | "
                 << std::setprecision(7) << value.value << ' ' << value.unit << " | "
                 << value.detail << '\n';
        // On restart, preserve the existing scorecard until a new row is appended.
        if (!values.empty() || state == "running")
            write_file(prefix + "/scorecard.txt", card.str());
        else
        {
            const auto previous = read_file(prefix + "/scorecard.txt");
            const auto newline = previous.find('\n');
            const std::string title = "Red Pitaya BIST 1.0.0 | run " + run_id + " | " + state;
            write_file(prefix + "/scorecard.txt",
                       title + (newline == std::string::npos ? "\n" : previous.substr(newline)));
        }
    }
    void log(std::string text)
    {
        std::lock_guard lock(mutex);
        log_text += timestamp() + " " + text + "\n";
        persist();
    }
    void add(std::string name, double value, std::string unit, bool pass, std::string detail,
             std::string domain = "os", std::string extra = "", bool headline = true)
    {
        std::lock_guard lock(mutex);
        Metric metric{std::move(name),
                      std::move(domain),
                      std::move(unit),
                      std::move(detail),
                      std::move(extra),
                      value,
                      pass,
                      headline};
        // Append to the persisted JSON array, also after instrument reconstruction.
        metrics.pop_back();
        if (metrics.size() > 1)
            metrics += ",";
        metrics += metric.json() + "]";
        if (values.empty() && state != "running")
        {
            const auto old = read_file(prefix + "/scorecard.txt");
            write_file(prefix + "/scorecard.txt", old + (metric.pass ? "PASS " : "FAIL ") +
                                                      metric.domain + " | " + metric.name + " | " +
                                                      number(value) + " " + metric.unit + " | " +
                                                      metric.detail + "\n");
        }
        else
            values.push_back(metric);
        log_text += timestamp() + " " + (pass ? "PASS " : "FAIL ") + metric.name +
                    " expected=" + metric.detail + " observed=" + number(value) + " " +
                    metric.unit + "\n";
        persist();
    }
    void snapshot()
    {
        const uint32_t id = ctl.read<reg::snapshot>() + 1;
        ctl.write<reg::snapshot>(id);
        barrier();
        const auto deadline = Clock::now() + std::chrono::milliseconds(20);
        while (sts.read<reg::snapshot_id>() != id && Clock::now() < deadline)
            std::this_thread::yield();
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
        // /proc/iomem proves exclusion; the boot DT's no-map reservation is checked by the host
        // too.
        add("dma_reservation", reserved ? 1 : 0, "bool", reserved && boot_reservation,
            "boot no-map reservation and fixed 8 MiB buffer must not intersect System RAM", "os",
            "\"iomem\":" + quote(read_file("/proc/iomem")) +
                ",\"reservation_reg_bytes\":" + std::to_string(tree.size()),
            false);
        snapshot();
        const uint64_t cycle_start =
            sts.read<reg::cycles0>() | (uint64_t(sts.read<reg::cycles1>()) << 32);
        const auto clock_start = Clock::now();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        snapshot();
        const uint64_t cycle_end =
            sts.read<reg::cycles0>() | (uint64_t(sts.read<reg::cycles1>()) << 32);
        const double measured_clock = double(cycle_end - cycle_start) / seconds(clock_start);
        add("converter_clock", measured_clock, "Hz",
            measured_clock > 123750000 && measured_clock < 126250000,
            "PL cycle counter against onboard monotonic clock, expected 125 MHz within 1%",
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
        auto start = Clock::now();
        for (unsigned i = 0; i < n && !cancelled; ++i)
            ctl.write<reg::scratch>(pattern(i, 0x1234));
        const double writes = seconds(start);
        start = Clock::now();
        uint32_t last = 0;
        for (unsigned i = 0; i < n && !cancelled; ++i)
            last = sts.read<reg::scratch_echo>();
        const double reads = seconds(start);
        if (last != pattern(n - 1, 0x1234))
            ++errors;
        for (unsigned i = 0; i < 256 && !cancelled; ++i)
        {
            const uint32_t expected = pattern(i, 0xabcdef);
            ctl.write<reg::scratch>(expected);
            barrier();
            if (ctl.read<reg::scratch>() != expected || sts.read<reg::scratch_echo>() != expected)
                ++errors;
        }
        add("register_read", reads * 1e9 / n, "ns/read", errors == 0,
            "20000 status reads; 256 exact write/read/echo cases");
        add("register_write", writes * 1e9 / n, "ns/write", errors == 0, "20000 config writes",
            "os", "\"integrity_errors\":" + std::to_string(errors));
        constexpr unsigned loops = 256, words = 1024;
        start = Clock::now();
        for (unsigned j = 0; j < loops && !cancelled; ++j)
            for (unsigned i = 0; i < words; ++i)
                bram.write_reg(i * 4, pattern(i, j));
        barrier();
        const double bw = seconds(start);
        start = Clock::now();
        for (unsigned j = 0; j < loops && !cancelled; ++j)
            for (unsigned i = 0; i < words; ++i)
                errors += bram.read_reg(i * 4) != pattern(i, loops - 1);
        const double br = seconds(start);
        add("bram_write", double(loops * words * 4) / bw / 1e6, "MB/s", errors == 0,
            "4 KiB bulk window, 256 passes, WC drain included");
        add("bram_read", double(loops * words * 4) / br / 1e6, "MB/s", errors == 0,
            "full data comparison on every pass", "os",
            "\"integrity_errors\":" + std::to_string(errors));
    }
    void dma_test(const std::string& workload, double duration)
    {
        uint64_t errors = 0, transferred = 0;
        unsigned interrupts = 0, iterations = 0;
        double transfer_seconds = 0;
        std::vector<double> latencies;
        const auto start = Clock::now();
        do
        {
            if (!dma.reset())
            {
                ++errors;
                break;
            }
            const uint32_t seed = 0xb1570000U + iterations;
            dma.fill(Dma::bytes, seed);
            const auto result = dma.transfer(Dma::bytes, true, true);
            errors += result.errors + (!result.ok);
            interrupts += result.interrupts;
            transfer_seconds += result.seconds;
            latencies.push_back(result.seconds * 1e3);
            errors += dma.verify(Dma::bytes, seed);
            transferred += result.ok ? Dma::bytes : 0;
            ++iterations;
            if (!result.ok)
                break;
        } while (!cancelled && seconds(start) < duration);
        snapshot();
        const uint64_t tx =
            sts.read<reg::tx_words0>() | (uint64_t(sts.read<reg::tx_words1>()) << 32);
        const uint64_t rx =
            sts.read<reg::rx_words0>() | (uint64_t(sts.read<reg::rx_words1>()) << 32);
        errors += sts.read<reg::discontinuities>() + sts.read<reg::overflow>();
        if (tx != Dma::bytes / 4 || rx != Dma::bytes / 4 ||
            sts.read<reg::packets>() != Dma::bytes / Dma::chunk)
            ++errors;
        add("dma_" + workload,
            transfer_seconds > 0 ? double(transferred) / transfer_seconds / 1e6 : 0, "MB/s",
            errors == 0 && iterations > 0,
            "2 MiB full duplex SG, 32 x 64 KiB descriptors, exact pattern and descriptor lengths; "
            "buffers reused",
            "os",
            "\"errors\":" + std::to_string(errors) + ",\"interrupts\":" +
                std::to_string(interrupts) + ",\"iterations\":" + std::to_string(iterations) +
                ",\"bytes\":" + std::to_string(transferred) +
                ",\"latency_ms\":" + distribution(latencies) +
                ",\"tx_words\":" + std::to_string(tx) + ",\"rx_words\":" + std::to_string(rx) +
                ",\"fifo_overflow\":" + std::to_string(sts.read<reg::overflow>()));
        dma.stop();
    }
    std::array<SerialResult, 2> serial_exchange(size_t bytes, bool roundtrip)
    {
        SerialEndpoint<mem::uart_a> a(cancelled);
        SerialEndpoint<mem::uart_b> b(cancelled);
        std::array<SerialResult, 2> result;
        if (!a.open() || !b.open())
        {
            result[0].errors = 1;
            return result;
        }
        if (!a.idle_timeout() || !b.idle_timeout())
        {
            result[0].errors = 1;
            return result;
        }
        a.reset();
        b.reset();
        std::barrier ready(2);
        std::thread ta([&] { result[0] = a.exchange(bytes, 0, roundtrip, &ready); });
        std::thread tb([&] { result[1] = b.exchange(bytes, 1, roundtrip, &ready); });
        ta.join();
        tb.join();
        return result;
    }
    void serial_test(const std::string& workload)
    {
        auto result = serial_exchange(32768, false);
        uint64_t errors = 0;
        double elapsed = 0;
        size_t bytes = 0;
        unsigned interrupts = 0;
        std::vector<double> observation;
        std::ostringstream endpoints;
        endpoints << '[';
        for (unsigned i = 0; i < 2; ++i)
        {
            auto& r = result[i];
            errors += r.errors;
            elapsed = std::max(elapsed, r.seconds);
            bytes += r.received;
            interrupts += r.interrupts;
            observation.insert(observation.end(), r.observation_us.begin(), r.observation_us.end());
            if (i)
                endpoints << ',';
            endpoints << "{\"channel\":" << i << ",\"received\":" << r.received
                      << ",\"transmitted\":" << r.transmitted << ",\"errors\":" << r.errors
                      << ",\"interrupts\":" << r.interrupts << ",\"poll_timeouts\":" << r.timeouts
                      << ",\"scheduling_error\":" << r.scheduling_error
                      << ",\"frame_cycles\":" << r.frame_cycles
                      << ",\"observation_us\":" << distribution(r.observation_us)
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
        add("uart_" + workload, elapsed > 0 ? double(bytes) / elapsed / 1000 : 0, "kB/s aggregate",
            errors == 0 && interrupts > 4,
            "32768 deterministic bytes each way at 460800 baud 8N1; sequence checked byte for byte",
            "os", "\"channels\":" + endpoints.str() + ",\"errors\":" + std::to_string(errors));
        {
            const auto exchange = serial_exchange(32 * 24, true);
            auto rtt = exchange[0].roundtrip_us;
            const uint64_t roundtrip_errors = exchange[0].errors + exchange[1].errors;
            std::sort(rtt.begin(), rtt.end());
            add("uart_roundtrip_" + workload, rtt.empty() ? 0 : rtt[rtt.size() / 2], "us",
                roundtrip_errors == 0 && rtt.size() == 24,
                "24 request/reply frames, 32 bytes each, persistent UIO workers; idle IRQ timeout "
                "before traffic",
                "os", "\"latency_us\":" + distribution(rtt), workload == "isolated");
        }
        if (workload == "isolated")
        {
            const bool baud_ok = result[0].frame_cycles >= 2712 && result[0].frame_cycles <= 2713 &&
                                 result[1].frame_cycles >= 2712 && result[1].frame_cycles <= 2713;
            add("uart_baud", result[0].frame_cycles ? 1250000000.0 / result[0].frame_cycles : 0,
                "baud", baud_ok,
                "independent PL frame counter: ten bit-times must take 2712 or 2713 ADC-clock "
                "cycles");
            std::sort(observation.begin(), observation.end());
            add("uio_observation", observation.empty() ? 0 : observation[observation.size() / 2],
                "us", !observation.empty(),
                "PL IRQ assertion to userspace clock-register read; includes AXI read overhead",
                "os", "\"latency_us\":" + distribution(observation));
        }
    }
    void rpc_test(const std::string& workload, bool bulk = false)
    {
        const auto result = rpc_benchmark(bulk, .5, cancelled);
        add("rpc_" + (bulk ? std::string("bulk") : workload),
            result.elapsed > 0
                ? (bulk ? double(result.bytes) / 1e6 : double(result.calls)) / result.elapsed
                : 0,
            bulk ? "MB/s" : "requests/s", result.errors == 0 && result.calls > 0,
            bulk ? "onboard TCP RPC: 1024-word generated vector reply, exact payload checked"
                 : "onboard TCP RPC: generated ping decode, dispatch and reply; one request "
                   "outstanding",
            "os",
            "\"errors\":" + std::to_string(result.errors) + ",\"calls\":" +
                std::to_string(result.calls) + ",\"latency_us\":" + distribution(result.us));
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
            "SG DMA upload into the CPU-accessible BRAM must match all 1024 packed DAC words",
            "instrument", "", false);
        ctl.write<reg::mode>(2);
        ctl.write<reg::samples>(16384);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        const auto capture = dma.transfer(65536, false, true, true);
        snapshot();
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
            const bool healthy = capture.ok && tone.amplitude > 2500 && tone.amplitude < 6000 &&
                                 snr > 30 && tone.clipping < 1 && sts.read<reg::overflow>() == 0;
            add("analog_" + std::to_string(channel), tone.amplitude, "ADC codes peak",
                connected ? healthy : true,
                connected ? "DAC to ADC cable; coherent 3.90625 MHz tone, 15.625 MS/s capture; "
                            "gain is uncalibrated"
                          : "external cable absent; analog validation skipped, input noise still "
                            "measured",
                "instrument",
                "\"status\":" + quote(connected ? (healthy ? "pass" : "fail") : "skipped") +
                    ",\"mean_codes\":" + number(tone.mean) +
                    ",\"residual_rms_codes\":" + number(tone.residual_rms) +
                    ",\"snr_db\":" + number(snr) + ",\"clip_count\":" + number(tone.clipping) +
                    ",\"dma_ok\":" + (capture.ok ? "true" : "false") +
                    ",\"fifo_overflow\":" + std::to_string(sts.read<reg::overflow>()),
                false);
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
    void run(unsigned mask)
    {
        // The previous run and shutdown leave the shared path in reset.
        ctl.write<reg::mode>(0);
        barrier();
        log("Validation started. Fixed workload v1: bounded waits; timed regions exclude report "
            "writes.");
        mappings();
        if (!cancelled)
            registers();
        const bool ready = dma.open();
        add("dma_uio_interrupt_binding", ready ? 1 : 0, "bool", ready,
            "DMA must expose UIO interrupt and register map", "os", "", false);
        if (ready && !cancelled)
            dma_test("isolated", 1.0);
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
            std::atomic<bool> burn_done{false};
            std::atomic<uint64_t> burn_bytes{0};
            std::thread burn(
                [&]
                {
                    std::vector<uint64_t> memory(1024 * 1024, 0x42495354);
                    uint64_t accumulator = 0;
                    while (!burn_done && !cancelled)
                    {
                        for (auto& word : memory)
                        {
                            word = (word + accumulator) * 1664525U + 1013904223U;
                            accumulator ^= word;
                        }
                        burn_bytes.fetch_add(memory.size() * 16, std::memory_order_relaxed);
                    }
                    if (accumulator == 0)
                        burn_bytes.fetch_add(0);
                });
            const auto combined_start = Clock::now();
            std::thread serial([&] { serial_test("combined"); });
            std::thread rpc([&] { rpc_test("combined"); });
            dma_test("combined", 1.5);
            serial.join();
            rpc.join();
            burn_done = true;
            burn.join();
            getrusage(RUSAGE_SELF, &after);
            const double elapsed = seconds(combined_start);
            {
                std::lock_guard lock(mutex);
                defer_io = false;
                persist();
            }
            const double cpu = (after.ru_utime.tv_sec - before.ru_utime.tv_sec) +
                               (after.ru_utime.tv_usec - before.ru_utime.tv_usec) / 1e6 +
                               (after.ru_stime.tv_sec - before.ru_stime.tv_sec) +
                               (after.ru_stime.tv_usec - before.ru_stime.tv_usec) / 1e6;
            add("cpu_memory_load", double(burn_bytes) / elapsed / 1e6, "MB/s read+write",
                burn_bytes > 0,
                "one CPU/memory worker over 8 MiB during concurrent DMA/UART/RPC; server RSS and "
                "scheduling recorded",
                "os",
                "\"process_cpu_seconds\":" + number(cpu) + ",\"elapsed_seconds\":" +
                    number(elapsed) + ",\"max_rss_kib\":" + std::to_string(after.ru_maxrss) +
                    ",\"major_faults\":" + std::to_string(after.ru_majflt - before.ru_majflt) +
                    ",\"involuntary_switches\":" +
                    std::to_string(after.ru_nivcsw - before.ru_nivcsw) +
                    ",\"meminfo\":" + quote(read_file("/proc/meminfo")));
        }
        dma.stop();
        add("suite_duration", seconds(beginning), "s", seconds(beginning) < 120 && !cancelled,
            "onboard suite must finish within 120 s; client validation including reboot within 180 "
            "s",
            "os", "", false);
        {
            std::lock_guard lock(mutex);
            state = cancelled ? "cancelled" : "complete";
            persist();
        }
        active = false;
    }
};

Bist::Bist() : impl(std::make_unique<Impl>()) {}
Bist::~Bist() = default;
bool Bist::start(uint32_t analog_mask)
{
    if (analog_mask > 3 || impl->active)
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
    impl->storage_ok = true;
    impl->metrics = "[]";
    impl->values.clear();
    impl->log_text.clear();
    impl->state = "running";
    utsname system{};
    uname(&system);
    impl->metadata =
        "{\"bist_version\":\"1.0.0\",\"workload_version\":1,\"sdk_version\":" +
        quote(KOHERON_VERSION) + ",\"bist_build_id\":" + quote(BIST_BUILD_ID) +
        ",\"started_utc\":" + quote(timestamp()) + ",\"kernel\":" + quote(system.release) +
        ",\"os_release\":" + quote(read_file("/etc/os-release")) +
        ",\"cpuinfo\":" + quote(read_file("/proc/cpuinfo")) +
        ",\"kernel_cmdline\":" + quote(read_file("/proc/cmdline")) +
        ",\"boot_id\":" + quote(read_file("/proc/sys/kernel/random/boot_id")) +
        ",\"board\":\"STEMlab 125-14 Gen1\",\"fpga_identity\":" +
        std::to_string(impl->sts.read<reg::identity>()) + ",\"fpga_dna\":" +
        quote(std::to_string(impl->sts.read<reg::dna0>() |
                             (uint64_t(impl->sts.read<reg::dna1>()) << 32))) +
        ",\"fpga_sha256\":" + quote(digest("/tmp/live-instrument/bist.bit.bin")) +
        ",\"server_sha256\":" + quote(digest("/proc/" + std::to_string(getpid()) + "/exe")) +
        ",\"instrument_manifest\":" + quote(read_file("/tmp/live-instrument/instrument.json")) +
        ",\"parameters\":{\"analog_mask\":" + std::to_string(analog_mask) +
        ",\"adc_clock_hz\":125000000,\"capture_decimation\":8,\"baud\":460800,\"framing\":\"8N1\","
        "\"dma_bytes\":2097152,\"dma_descriptors\":32,\"dma_chunk_bytes\":65536,\"uart_bytes_per_"
        "direction\":32768,\"uart_fifo_bytes\":64,\"uart_scheduler\":\"SCHED_FIFO\",\"uart_"
        "priority\":1,\"uart_worker_cpus\":[0,1]},\"comparability\":\"Require matching FPGA "
        "SHA256, workload version, "
        "parameters and wiring; absent analog cables are skipped\"}";
    impl->write_file(std::string(root) + "/latest", impl->run_id);
    impl->persist();
    impl->cancelled = false;
    impl->active = true;
    impl->beginning = Clock::now();
    impl->worker = std::thread([this, analog_mask] { impl->run(analog_mask); });
    return true;
}
void Bist::cancel()
{
    impl->cancelled = true;
}
bool Bist::running()
{
    return impl->active;
}
double Bist::get_boot_seconds()
{
    timespec now{};
    clock_gettime(CLOCK_BOOTTIME, &now);
    return double(now.tv_sec) + double(now.tv_nsec) / 1e9;
}
std::string Bist::get_report()
{
    std::lock_guard lock(impl->mutex);
    return impl->report_unlocked();
}
std::string Bist::get_scorecard()
{
    std::lock_guard lock(impl->mutex);
    return read_file(impl->prefix + "/scorecard.txt");
}
std::string Bist::get_log(uint32_t cursor)
{
    std::lock_guard lock(impl->mutex);
    const size_t begin = std::min(size_t(cursor), impl->log_text.size()),
                 end = std::min(begin + 16384, impl->log_text.size());
    return "{\"run_id\":" + quote(impl->run_id) + ",\"cursor\":" + std::to_string(end) +
           ",\"state\":" + quote(impl->state) +
           ",\"text\":" + quote(std::string_view(impl->log_text).substr(begin, end - begin)) + "}";
}
uint32_t Bist::ping(uint32_t value)
{
    return value ^ 0x42495354U;
}
std::vector<uint32_t> Bist::benchmark_reply(uint32_t words)
{
    std::vector<uint32_t> result(std::min(words, 16384U));
    for (size_t i = 0; i < result.size(); ++i)
        result[i] = pattern(uint32_t(i), 0x42495354U);
    return result;
}
bool Bist::record_startup(double network_seconds, double server_seconds, double boot_seconds)
{
    if (impl->active || impl->run_id.empty() || !std::isfinite(network_seconds) ||
        !std::isfinite(server_seconds) || !std::isfinite(boot_seconds) || network_seconds < 0 ||
        server_seconds < network_seconds || boot_seconds < 0)
        return false;
    impl->add("network_readiness", network_seconds, "s", true,
              "host monotonic time from reboot request to first new-boot ICMP echo reply");
    impl->add("server_readiness", server_seconds, "s", true,
              "host monotonic time from reboot request to generated BIST RPC readiness");
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
              "report retained across stop/start; overlay reapplied; DMA and both UARTs recover",
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
