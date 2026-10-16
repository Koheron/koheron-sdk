#include "full.hpp"
#include "../bist/checkpoint.hpp"
#include "../bist/metadata.hpp"
#include "../bist/uart.hpp"
#include "analog.hpp"
#include "completion.hpp"
#include "dma.hpp"
#include "platform.hpp"
#include "server/runtime/driver_manager.hpp"
#include "server/runtime/signal_handler.hpp"
#include <condition_variable>
#include <cstdio>
#include <filesystem>
#include <mutex>
#include <numbers>
#include <signal.h>
#include <sys/resource.h>
#include <sys/utsname.h>
#include <sys/wait.h>

namespace
{
using namespace bist;
constexpr auto root = "/var/lib/koheron/bist-full";
std::string digest(const std::string &path)
{
    auto *stream = popen(("sha256sum " + path + " 2>/dev/null").c_str(), "r");
    if (!stream)
        return "unavailable";
    char text[128]{};
    const bool ok = fgets(text, sizeof(text), stream);
    pclose(stream);
    return ok ? std::string(text, 64) : "unavailable";
}
size_t device_fds(const std::string &device)
{
    std::error_code error;
    size_t count = 0;
    for (std::filesystem::directory_iterator it("/proc/self/fd", error), end; !error && it != end;
         it.increment(error))
    {
        std::error_code link_error;
        const auto target = std::filesystem::read_symlink(it->path(), link_error);
        if (!link_error && target == device)
            ++count;
    }
    return count;
}
std::string array_append(std::string array, const std::string &value)
{
    array.pop_back();
    if (array.size() > 1)
        array += ',';
    return array + value + ']';
}
} // namespace
struct BistFull::Impl
{
    hw::Memory<mem::control> &ctl = hw::get_memory<mem::control>();
    hw::Memory<mem::status> &sts = hw::get_memory<mem::status>();
    hw::Memory<mem::bram> &bram = hw::get_memory<mem::bram>();
    hw::Memory<mem::buffers> &ram = hw::get_memory<mem::buffers>();
    std::atomic<bool> cancelled{false}, active{false};
    std::atomic<uint64_t> observers{0};
    std::mutex mutex;
    std::condition_variable completed;
    std::thread worker;
    std::string run_id, prefix, checks = "[]", metadata = "{}", state = "idle", host = "", log,
                                groups = "[]";
    bool storage_ok = true, reservation_ok = false;
    Clock::time_point beginning, deadline;
    uint32_t seed = 0;
    bist_full::Dma dma{cancelled};
    bist_full::Ddr ddr{cancelled};
    bist_full::Emit output()
    {
        return [this](const auto &g, const auto &n, bool pass, double value, const auto &unit,
                      const auto &detail, const auto &extra)
        { add(g, n, pass, value, unit, detail, extra); };
    }

    Impl()
    {
        run_id = read_file(std::string(root) + "/latest");
        if (run_id.empty() || run_id.find_first_not_of("0123456789") != std::string::npos)
        {
            run_id.clear();
            return;
        }
        prefix = std::string(root) + "/" + run_id;
        Checkpoint cp;
        if (!decode_checkpoint(read_file(prefix + "/checkpoint"), cp))
        {
            storage_ok = false;
            if (!decode_checkpoint(read_file(prefix + "/checkpoint.prev"), cp))
            {
                state = "corrupt";
                return;
            }
        }
        checks = cp[0];
        metadata = cp[1];
        run_id = cp[2];
        state = cp[3];
        host = cp[4];
        log = cp[5];
        groups = cp[6];
        if (state == "running")
        {
            state = "interrupted";
            persist();
        }
    }
    ~Impl()
    {
        cancelled = true;
        if (worker.joinable())
            worker.join();
        ddr.stop();
        dma.stop();
    }
    std::string report() const
    {
        return "{\"schema_version\":1,\"instrument\":\"bist-full\",\"run_id\":" + quote(run_id) +
               ",\"state\":" + quote(state) + ",\"storage_ok\":" + (storage_ok ? "true" : "false") +
               ",\"metadata\":" + metadata + ",\"checks\":" + checks +
               ",\"completed_groups\":" + groups +
               ",\"observer_requests\":" + std::to_string(observers.load()) +
               ",\"host_results_json\":" + quote(host) + "}";
    }
    bool write(const std::string &name, const std::string &text)
    {
        std::ofstream file(name, std::ios::binary | std::ios::trunc);
        file << text;
        file.close();
        if (!file)
            storage_ok = false;
        return bool(file);
    }
    void persist()
    {
        if (prefix.empty())
            return;
        const auto bytes =
            encode_checkpoint({checks, metadata, run_id, state, host, log, groups, "", ""});
        if (write(prefix + "/checkpoint.next", bytes))
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
                const int fd = ::open((prefix + "/checkpoint").c_str(), O_RDONLY | O_CLOEXEC);
                const int dir = ::open(prefix.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
                if (fd < 0 || dir < 0)
                    storage_ok = false;
                if (fd >= 0)
                {
                    if (::fsync(fd))
                        storage_ok = false;
                    ::close(fd);
                }
                if (dir >= 0)
                {
                    if (::fsync(dir))
                        storage_ok = false;
                    ::close(dir);
                }
            }
        }
        write(prefix + "/report.json", report());
        write(prefix + "/log.txt", log);
    }
    void add(const std::string &group, const std::string &name, bool pass, double value,
             const std::string &unit, const std::string &detail, const std::string &extra = "",
             bool skip = false)
    {
        std::lock_guard lock(mutex);
        const std::string status = skip ? "skipped" : pass ? "pass" : "fail";
        checks = array_append(checks,
                              "{\"group\":" + quote(group) + ",\"name\":" + quote(name) +
                                  ",\"status\":" + quote(status) + ",\"value\":" + number(value) +
                                  ",\"unit\":" + quote(unit) + ",\"detail\":" + quote(detail) +
                                  ",\"elapsed_ms\":" + number(seconds(beginning) * 1000) +
                                  (extra.empty() ? "" : "," + extra) + "}");
        log += number(seconds(beginning)) + " " + status + " " + group + "/" + name + " " +
               number(value) + " " + unit + " " + detail + "\n";
        persist();
    }
    bool available() const { return !cancelled && Clock::now() < deadline && reservation_ok; }
    void group_done(const std::string &name)
    {
        std::lock_guard lock(mutex);
        groups = array_append(groups, quote(name));
        persist();
    }
    bool snapshot()
    {
        const auto token = ctl.read<reg::snapshot>() + 1;
        ctl.write<reg::snapshot>(token);
        barrier();
        const auto end = Clock::now() + std::chrono::milliseconds(10);
        while (sts.read<reg::snapshot_id>() != token && Clock::now() < end)
            std::this_thread::yield();
        return sts.read<reg::snapshot_id>() == token;
    }
    bool reservation_valid()
    {
        std::ifstream iomem("/proc/iomem");
        const std::string reservation =
            "/sys/firmware/devicetree/base/reserved-memory/dma-buffer@18000000";
        const auto reg = read_file(reservation + "/reg");
        uint64_t base = 0, size = 0;
        if (reg.size() == 8)
            for (unsigned i = 0; i < 4; ++i)
            {
                base = (base << 8) | static_cast<unsigned char>(reg[i]);
                size = (size << 8) | static_cast<unsigned char>(reg[i + 4]);
            }
        return reg.size() == 8 && mem::buffers_addr >= base &&
               mem::buffers_addr + mem::buffers_range <= base + size &&
               std::filesystem::exists(reservation + "/no-map") &&
               hw::outside_system_ram(iomem, mem::buffers_addr, mem::buffers_range);
    }
    void platform()
    {
        bool mappings = true;
        std::string paths;
        const std::array<std::pair<uintptr_t, bool>, 8> regions{
            {{ctl.base_addr(), false},
             {sts.base_addr(), false},
             {dma.regs.base_addr(), false},
             {hw::get_memory<mem::uart_a>().base_addr(), false},
             {hw::get_memory<mem::uart_b>().base_addr(), false},
             {hw::get_memory<mem::xadc>().base_addr(), false},
             {bram.base_addr(), true},
             {ram.base_addr(), true}}};
        for (auto [address, wc] : regions)
        {
            const auto path = mapping_path(address);
            mappings &= path.starts_with(wc ? "/dev/mem_wc" : "/dev/uio");
            paths += path + "; ";
        }
        add("platform", "mapping_backends", mappings, 0, "errors", paths);
        add("platform", "fpga_identity", sts.read<reg::identity>() == 0x42494631,
            sts.read<reg::identity>(), "id", "new full-design identity 0x42494631");
        const bool reserved = reservation_valid();
        reservation_ok = reserved;
        add("platform", "dma_reservation", reserved, mem::buffers_range, "bytes",
            "boot no-map and exclusion from all Linux System RAM");
        const size_t before = device_fds(mapping_path(sts.base_addr()));
        bool cycles = true;
        for (unsigned i = 0; i < 32; ++i)
        {
            Uio<mem::status> map;
            cycles &= map.open() >= 0 && map.mmap() != nullptr;
        }
        const size_t after = device_fds(mapping_path(sts.base_addr()));
        add("platform", "mapping_lifetime_32", cycles && before == after,
            double(after) - double(before), "fds",
            "32 UIO map/unmap/open/close cycles; device-specific FD count excludes concurrent "
            "network sessions");
        const auto child = ::fork();
        bool protected_write = false;
        if (child == 0)
        {
            rlimit limit{0, 0};
            ::setrlimit(RLIMIT_CORE, &limit);
            ::signal(SIGSEGV, SIG_DFL);
            ::signal(SIGBUS, SIG_DFL);
            *static_cast<volatile uint32_t *>(sts.get_ptr<uint32_t>()) = 0;
            _exit(0);
        }
        if (child > 0)
        {
            int status = 0;
            const auto end = Clock::now() + std::chrono::milliseconds(100);
            pid_t observed = 0;
            while ((observed = ::waitpid(child, &status, WNOHANG)) == 0 && Clock::now() < end)
                ::usleep(1000);
            if (observed == 0)
            {
                ::kill(child, SIGKILL);
                ::waitpid(child, &status, 0);
            }
            else
                protected_write = observed == child && WIFSIGNALED(status) &&
                                  (WTERMSIG(status) == SIGSEGV || WTERMSIG(status) == SIGBUS);
        }
        add("platform", "read_only_write_denied", protected_write, protected_write, "bool",
            "isolated child must fault on an actual write through the read-only status mapping");
        dma.reset();
        snapshot();
        const auto begin = Clock::now();
        const auto first = sts.read<reg::cycles0>();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        snapshot();
        const double hz = double(sts.read<reg::cycles0>() - first) / seconds(begin);
        add("platform", "converter_clock", hz > 123000000 && hz < 127000000, hz, "Hz",
            "PL cycles against monotonic clock, tolerance 1.6%");
        auto &xadc = hw::get_memory<mem::xadc>();
        const double temp = double(xadc.read_reg(0x200)) * 503.975 / 65536 - 273.15;
        add("platform", "fpga_temperature", temp > -20 && temp < 100, temp, "C",
            "uncalibrated XADC health range");
        for (auto [offset, name, nominal] :
             std::array<std::tuple<unsigned, const char *, double>, 6>{{{0x204, "pl_vccint", 1.0},
                                                                        {0x208, "pl_vccaux", 1.8},
                                                                        {0x218, "pl_vccbram", 1.0},
                                                                        {0x234, "ps_vccint", 1.0},
                                                                        {0x238, "ps_vccaux", 1.8},
                                                                        {0x23c, "ps_vccmem", 1.5}}})
        {
            const double volts = double(xadc.read_reg(offset)) * 3 / 65536;
            add("platform", name, volts > nominal * .85 && volts < nominal * 1.15, volts, "V",
                "XADC rail sanity, nominal +/-15%");
        }
    }
    template <typename Memory>
    void memory_test(Memory &memory, uint32_t bytes, const std::string &name)
    {
        uint64_t errors = 0;
        const auto begin = Clock::now();
        for (uint32_t i = 0; i < bytes; i += 4)
            memory.write_reg(i, 0U);
        for (unsigned phase = 0; phase < 4 && available(); ++phase)
            for (uint32_t j = 0; j < bytes; j += 4)
            {
                const uint32_t i = phase < 2 ? j : bytes - 4 - j, expected = (phase % 2) ? ~0U : 0U;
                errors += memory.read_reg(i) != expected;
                memory.write_reg(i, ~expected);
            }
        for (uint32_t i = 0; i < bytes; i += 4)
            errors += memory.read_reg(i) != 0;
        add("memory", name + "_march_c_minus", !errors && available(), double(errors), "errors",
            "ascending/descending read-0/write-1 and read-1/write-0 over every word",
            "\"bytes\":" + std::to_string(bytes) + ",\"seconds\":" + number(seconds(begin)));
        errors = 0;
        for (unsigned bit = 0; bit < 32; ++bit)
            for (uint32_t value : {1U << bit, ~(1U << bit)})
            {
                memory.write_reg(bytes - 4, value);
                barrier();
                errors += memory.read_reg(bytes - 4) != value;
            }
        add("memory", name + "_walking_data", !errors, double(errors), "errors",
            "64 walking-one/zero data-bus trials at the final word");
        errors = 0;
        const uint32_t address_guard = seed ^ 0xa5a55a5aU;
        memory.write_reg(0, address_guard);
        for (uint32_t i = 4; i < bytes; i <<= 1)
        {
            memory.write_reg(i, i ^ seed);
            barrier();
            errors += memory.read_reg(0) != address_guard;
        }
        barrier();
        for (uint32_t i = 4; i < bytes; i <<= 1)
            errors += memory.read_reg(i) != (i ^ seed);
        add("memory", name + "_walking_address", !errors, double(errors), "errors",
            "independent values at every power-of-two address; alias detection");
        memory.write_reg(0, 0U);
        uint32_t expected = 0;
        errors = 0;
        for (unsigned lane = 0; lane < 4; ++lane)
        {
            memory.template write_reg<uint8_t>(lane, uint8_t(0x37 + lane));
            expected |= (0x37 + lane) << (8 * lane);
            barrier();
            errors += memory.read_reg(0) != expected;
        }
        memory.template write_reg<uint16_t>(0, 0xa55a);
        memory.template write_reg<uint16_t>(2, 0x1234);
        barrier();
        errors += memory.read_reg(0) != 0x1234a55a;
        add("memory", name + "_byte_halfword_strobes", !errors, double(errors), "errors",
            "byte-lane preservation and both aligned halfword writes");
        errors = 0;
        for (uint32_t i = 0; i < bytes; i += 4)
            memory.write_reg(i, pattern(i / 4, seed));
        barrier();
        for (uint32_t i = 0; i < bytes; i += 4)
            errors += memory.read_reg(i) != pattern(i / 4, seed);
        add("memory", name + "_address_pattern", !errors, double(errors), "errors",
            "full-window deterministic address-dependent pattern");
    }
    void registers()
    {
        uint64_t errors = 0;
        for (unsigned i = 0; i < 256; ++i)
        {
            const auto value = pattern(i, seed);
            ctl.write<reg::scratch>(value);
            barrier();
            errors += sts.read<reg::scratch_echo>() != value;
        }
        add("memory", "register_patterns", !errors, double(errors), "errors",
            "256 control/status echo patterns");
        errors = 0;
        for (uint32_t token : {1U, 0x7fffffffU, 0x80000000U, 0xfffffffeU, 0xffffffffU, 0U, 1U})
        {
            ctl.write<reg::snapshot>(token);
            barrier();
            const auto end = Clock::now() + std::chrono::milliseconds(10);
            while (sts.read<reg::snapshot_id>() != token && Clock::now() < end)
            {
            }
            errors += sts.read<reg::snapshot_id>() != token;
        }
        add("memory", "snapshot_token_wrap", !errors, double(errors), "errors",
            "coherent snapshots including signed boundary and uint32 wrap");
        memory_test(bram, mem::bram_range, "bram16k");
        if (available())
            memory_test(ram, 8 * 1024 * 1024, "ddr8m");
    }
    bool recover(const std::string &name)
    {
        const bool reset = dma.reset();
        ctl.write<reg::scratch>(seed);
        ctl.write<reg::packet_bytes>(256);
        dma.fill(256, seed, 0, 0);
        auto r = dma.transfer({256});
        const auto errors = dma.verify(256, seed, 0);
        const bool ok = reset && r.ok && !errors && !dma.guards(256, 0, 0);
        add("dma", name + "_recovery", ok, double(errors + r.errors), "errors",
            "reset followed by verified clean duplex DMA and intact guards");
        return ok;
    }
    void dma_matrix()
    {
        const bool opened = dma.open();
        add("dma", "uio_binding", opened, opened, "bool", "SG DMA register map and interrupt UIO");
        if (!opened)
            return;
        const std::vector<uint32_t> lengths{
            1,   2,   3,   4,   5,   7,   8,    15,   16,   31,   32,   63,    64,    65,
            127, 128, 255, 256, 511, 512, 1023, 1024, 4095, 4096, 4097, 65535, 65536, 65537};
        for (auto n : lengths)
        {
            if (!available())
                return;
            const bool reset = dma.reset();
            ctl.write<reg::scratch>(seed);
            ctl.write<reg::packet_bytes>(n);
            dma.fill(n, seed, 0, 0);
            auto r = dma.transfer({n});
            const auto errors = dma.verify(n, seed, 0), guard = dma.guards(n, 0, 0);
            const bool snap = snapshot();
            add("dma", "bytes_" + std::to_string(n),
                reset && r.ok && !errors && !guard && snap && !sts.read<reg::checker_errors>() &&
                    !sts.read<reg::keep_errors>() && !sts.read<reg::discontinuities>(),
                double(errors + r.errors + guard), "errors",
                "single packet, exact byte count, final TKEEP, PL checker and 64-byte guards",
                "\"seconds\":" + number(r.seconds));
        }
        for (uint32_t offset : {1U, 2U, 3U, 7U, 15U, 63U, 4093U, 4095U})
        {
            if (!available())
                return;
            dma.reset();
            ctl.write<reg::scratch>(seed);
            ctl.write<reg::packet_bytes>(4097);
            const uint32_t rx_offset = 4096 - offset;
            dma.fill(4097, seed, offset, rx_offset);
            auto r = dma.transfer({4097}, offset, rx_offset);
            const auto errors =
                dma.verify(4097, seed, rx_offset) + dma.guards(4097, offset, rx_offset);
            add("dma", "unaligned_" + std::to_string(offset), r.ok && !errors,
                double(errors + r.errors), "errors",
                "different TX/RX byte offsets with DRE, including 4 KiB page crossings");
        }
        for (unsigned count : {2U, 3U, 31U, 32U, 63U, 64U})
        {
            if (!available())
                return;
            std::vector<uint32_t> chunks(count, 1024);
            chunks.back() = 1027;
            const auto n = std::accumulate(chunks.begin(), chunks.end(), 0U);
            dma.reset();
            ctl.write<reg::scratch>(seed);
            ctl.write<reg::packet_bytes>(n);
            dma.fill(n, seed, 0, 0);
            auto r = dma.transfer(chunks);
            const auto errors = dma.verify(n, seed, 0) + dma.guards(n, 0, 0);
            add("dma", "sg_" + std::to_string(count), r.ok && !errors, double(errors + r.errors),
                "errors",
                "one packet spans a circularly-linked descriptor chain; only final descriptor "
                "terminates packet");
        }
        for (uint32_t stalls : {0x0080U, 0x8000U, 0xf0f0U})
        {
            dma.reset();
            ctl.write<reg::scratch>(seed);
            ctl.write<reg::packet_bytes>(65536);
            ctl.write<reg::stalls>(stalls);
            dma.fill(65536, seed, 0, 0);
            auto r = dma.transfer({65536});
            snapshot();
            const auto errors = dma.verify(65536, seed, 0) + dma.guards(65536, 0, 0);
            add("dma", "backpressure_" + std::to_string(stalls),
                r.ok && !errors && sts.read<reg::input_stalls>() > 0, double(errors + r.errors),
                "errors", "elastic VALID/data/TKEEP/TLAST under deterministic 50% and 94% stalls",
                "\"input_stall_cycles\":" + std::to_string(sts.read<reg::input_stalls>()));
        }
        for (unsigned reuse = 0; reuse < 16 && available(); ++reuse)
        {
            dma.reset();
            const auto key = seed + reuse;
            ctl.write<reg::scratch>(key);
            ctl.write<reg::packet_bytes>(8192);
            dma.fill(8192, key, 0, 0);
            auto r = dma.transfer({4096, 4096});
            const auto errors = dma.verify(8192, key, 0) + dma.guards(8192, 0, 0);
            add("dma", "descriptor_reuse_" + std::to_string(reuse), r.ok && !errors,
                double(errors + r.errors), "errors",
                "same DDR buffers/descriptors reused with a distinct pattern");
        }
        uint32_t random = seed | 1;
        auto next = [&]
        {
            random ^= random << 13;
            random ^= random >> 17;
            random ^= random << 5;
            return random;
        };
        uint64_t randomized_errors = 0, randomized_bytes = 0;
        unsigned trials = 0;
        for (; trials < 32 && available(); ++trials)
        {
            const uint32_t n = 4097 + (next() & 32767), tx_offset = next() & 63,
                           rx_offset = next() & 63, key = next();
            std::vector<uint32_t> tx_chunks, rx_chunks;
            for (bool transmit : {false, true})
            {
                auto &chain = transmit ? tx_chunks : rx_chunks;
                uint32_t remaining = n;
                while (remaining)
                {
                    const auto chunk = chain.size() == 63
                                           ? remaining
                                           : std::min(remaining, transmit ? 4 * (1 + (next() & 511))
                                                                          : 1 + (next() & 2047));
                    chain.push_back(chunk);
                    remaining -= chunk;
                }
            }
            dma.reset();
            ctl.write<reg::scratch>(key);
            ctl.write<reg::packet_bytes>(n);
            dma.fill(n, key, tx_offset, rx_offset);
            const auto r = dma.transfer(tx_chunks, tx_offset, rx_offset, true, true, false, 250, 0,
                                        false, rx_chunks);
            snapshot();
            randomized_errors += !r.ok;
            randomized_errors +=
                dma.verify(n, key, rx_offset) + dma.guards(n, tx_offset, rx_offset);
            randomized_errors += sts.read<reg::checker_errors>() + sts.read<reg::keep_errors>() +
                                 sts.read<reg::discontinuities>();
            randomized_bytes += n;
        }
        add("dma", "randomized_asymmetric_sg", trials == 32 && !randomized_errors,
            double(randomized_errors), "errors",
            "32 seeded packets: independently partitioned TX/RX chains, different DRE offsets, "
            "every descriptor status, exact bytes, PL checker and guards",
            "\"trials\":" + std::to_string(trials) + ",\"bytes\":" +
                std::to_string(randomized_bytes) + ",\"seed\":" + std::to_string(seed));
        // Independent PL source and sink isolate each DMA direction.
        for (bool source : {false, true})
        {
            dma.reset();
            ctl.write<reg::scratch>(seed);
            ctl.write<reg::packet_bytes>(4097);
            ctl.write<reg::mode>(source ? 3 : 4);
            dma.fill(4097, seed, 0, 0);
            auto r = dma.transfer({4097}, 0, 0, !source, source, source);
            snapshot();
            const auto errors =
                source ? dma.verify(4097, seed, 0) : uint64_t(sts.read<reg::checker_errors>());
            add("dma", source ? "pl_generator_rx_only" : "pl_checker_tx_only", r.ok && !errors,
                double(errors + r.errors), "errors",
                "independent direction without digital loopback");
        }
        // Measure a larger aligned transfer separately from preparation/verification.
        dma.reset();
        ctl.write<reg::scratch>(seed);
        ctl.write<reg::packet_bytes>(bist_full::Dma::maximum);
        dma.fill(bist_full::Dma::maximum, seed, 0, 0);
        auto r = dma.transfer(std::vector<uint32_t>(32, 65536));
        const auto errors = dma.verify(bist_full::Dma::maximum, seed, 0) +
                            dma.guards(bist_full::Dma::maximum, 0, 0);
        add("dma", "duplex_2m_capacity", r.ok && !errors, bist_full::Dma::maximum / r.seconds / 1e6,
            "MB/s payload",
            "2 MiB, transfer-completion time excludes preparation and comparison; payload counts "
            "one direction");
    }
    void faults()
    {
        for (unsigned fault : {1U, 2U})
        {
            if (!available())
                return;
            dma.reset();
            ctl.write<reg::scratch>(seed);
            ctl.write<reg::packet_bytes>(256);
            dma.fill(256, seed, 0, 0);
            const auto r = dma.transfer({256}, 0, 0, true, true, false, 100, fault);
            add("dma", fault == 1 ? "reject_zero_length_bd" : "reject_completed_bd",
                !r.ok && ((r.tx_status | r.rx_status) & 0x770U), r.tx_status | r.rx_status,
                "status", "expected SG/internal DMA fault must be signalled, not merely time out");
            recover("descriptor_fault_" + std::to_string(fault));
        }
        dma.reset();
        ctl.write<reg::scratch>(seed);
        ctl.write<reg::packet_bytes>(256);
        dma.fill(256, seed, 0, 0);
        ram.write_reg<uint8_t>(bist_full::Dma::tx_base + 12,
                               ram.read_reg<uint8_t>(bist_full::Dma::tx_base + 12) ^ 1);
        auto r = dma.transfer({256});
        snapshot();
        const auto detected = sts.read<reg::checker_errors>();
        add("dma", "detect_tx_corruption", r.ok && detected == 1 && dma.verify(256, seed, 0) == 1,
            detected, "bad words",
            "deliberate source bit flip detected independently by PL and CPU");
        recover("tx_corruption");
        dma.reset();
        ctl.write<reg::scratch>(seed);
        ctl.write<reg::packet_bytes>(256);
        ctl.write<reg::fault>(1);
        dma.fill(256, seed, 0, 0);
        r = dma.transfer({256});
        snapshot();
        add("dma", "detect_post_checker_corruption",
            r.ok && dma.verify(256, seed, 0) == 1 && !sts.read<reg::checker_errors>(),
            dma.verify(256, seed, 0), "bad bytes",
            "PL output bit flip must be caught by CPU despite a clean input checker");
        recover("output_corruption");
        for (unsigned fault : {2U, 4U})
        {
            dma.reset();
            ctl.write<reg::mode>(3);
            ctl.write<reg::fault>(fault);
            ctl.write<reg::scratch>(seed);
            ctl.write<reg::packet_bytes>(256);
            dma.fill(256, seed, 0, 0);
            r = dma.transfer({256}, 0, 0, false, true, true, 100);
            const bool captured = snapshot();
            const bool missing_end =
                !r.ok && !(ram.read_reg(bist_full::Dma::rx_bd + 28) & 0x04000000U) && captured &&
                sts.read<reg::rx_words0>() == 64 && sts.read<reg::packets>() == 0;
            const bool detected = fault == 2 ? r.completed && r.received == 16 : missing_end;
            add("dma", fault == 2 ? "detect_early_tlast" : "detect_missing_tlast",
                detected && !dma.guards(256, 0, 0), r.received, "bytes",
                "early packet is short; missing TLAST must hit bounded completion timeout");
            recover("tlast_" + std::to_string(fault));
        }
        dma.reset();
        ctl.write<reg::scratch>(seed);
        ctl.write<reg::packet_bytes>(256);
        dma.fill(256, seed, 0, 0);
        r = dma.transfer({256}, 0, 0, true, true, false, 100, 0, true);
        add("dma", "masked_uio_irq_timeout",
            !r.completed && r.received == 256 && r.interrupts == 0 && r.seconds >= .08, r.seconds,
            "s",
            "descriptors completed but masked UIO interrupt must not count as observed completion");
        recover("masked_irq");
        add("dma", "reject_invalid_requests",
            !bist_full::Dma::valid({}, 0) && !bist_full::Dma::valid({0}, 0) &&
                !bist_full::Dma::valid({bist_full::Dma::maximum + 1}, 0) &&
                !bist_full::Dma::valid(std::vector<uint32_t>(65, 4), 0) &&
                !bist_full::Dma::valid({4}, 4097),
            5, "cases", "invalid sizes, chain count and offsets rejected before MMIO");
    }
    void interrupts()
    {
        dma.reset();
        Uio<mem::status> irq;
        bool ok = irq.open() >= 0;
        ctl.write<reg::irq_period>(125000);
        barrier();
        unsigned events = 0;
        const auto begin = Clock::now();
        for (unsigned i = 0; i < 32 && ok && available(); ++i)
        {
            ok = irq.arm_irq() && irq.wait_for_irq(std::chrono::milliseconds(10)) > 0 &&
                 sts.read<reg::irq_pending>() == 1;
            events += ok;
            ctl.write<reg::irq_ack>(ctl.read<reg::irq_ack>() + 1);
            barrier();
        }
        ctl.write<reg::irq_period>(0);
        barrier();
        add("irq", "level_rearm_32", ok && events == 32, events, "events",
            "1 kHz level IRQ, hardware cause acknowledged before each UIO rearm",
            "\"seconds\":" + number(seconds(begin)));
        ctl.write<reg::irq_period>(125000);
        barrier();
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
        snapshot();
        const auto pending = sts.read<reg::irq_pending>(), generated = sts.read<reg::irq_count>();
        const bool reassert = irq.arm_irq() && irq.wait_for_irq(std::chrono::milliseconds(10)) > 0;
        ctl.write<reg::irq_period>(0);
        ctl.write<reg::irq_ack>(ctl.read<reg::irq_ack>() + 1);
        barrier();
        while (irq.wait_for_irq(std::chrono::milliseconds(0)) > 0)
        {
        }
        const bool quiet = irq.arm_irq() && irq.wait_for_irq(std::chrono::milliseconds(5)) == 0;
        add("irq", "masked_level_retention", pending == 1 && generated >= 8 && reassert && quiet,
            generated, "generated events",
            "pending level retained while UIO is masked; rearm redelivers; disabling/ack clears "
            "it");
    }
    void serial()
    {
        SerialEndpoint<mem::uart_a> a(cancelled);
        SerialEndpoint<mem::uart_b> b(cancelled);
        const bool open = a.open() && b.open();
        add("uart", "uio_binding", open, open, "bool", "two independent UART UIO devices");
        if (!open)
            return;
        const bool idle = a.idle_timeout() && b.idle_timeout();
        add("uart", "idle_timeout", idle, idle, "bool",
            "idle UIO waits time out without spurious IRQ");
        for (size_t n : {size_t(1), size_t(31), size_t(32), size_t(63), size_t(64), size_t(65),
                         size_t(127), size_t(128), size_t(129), size_t(32768)})
        {
            if (!available())
                return;
            a.reset();
            b.reset();
            SerialResult ar, br;
            StartGate gate{2};
            std::thread first([&] { ar = a.exchange(n, 0, false, &gate); }),
                second([&] { br = b.exchange(n, 1, false, &gate); });
            first.join();
            second.join();
            const auto errors = ar.errors + br.errors;
            add("uart", "duplex_bytes_" + std::to_string(n),
                !errors && ar.received == n && br.received == n, double(errors), "errors",
                "simultaneous deterministic traffic, FIFO-1/FIFO/FIFO+1 and wrap boundaries",
                "\"aggregate_kB_s\":" +
                    number(2 * double(n) / std::max(ar.seconds, br.seconds) / 1000));
        }
        a.reset();
        b.reset();
        SerialResult ar, br;
        StartGate request_gate{2};
        std::thread first([&] { ar = a.exchange(32 * 100, 0, true, &request_gate); }),
            second([&] { br = b.exchange(32 * 100, 1, true, &request_gate); });
        first.join();
        second.join();
        add("uart", "request_reply_100", !ar.errors && !br.errors && ar.roundtrip_us.size() == 100,
            ar.roundtrip_us.empty() ? 0 : ar.roundtrip_us[0], "us first RTT",
            "100 persistent-worker request/reply frames",
            "\"latency_us\":" + distribution(ar.roundtrip_us));
        auto &tx = hw::get_memory<mem::uart_a>();
        auto &rx = hw::get_memory<mem::uart_b>();
        // Hold the physical internal RX wire low, beyond a complete stop bit.
        auto framing = [&](auto &endpoint, auto &irq, unsigned direction)
        {
            a.reset();
            b.reset();
            const bool bound = irq.open() >= 0;
            endpoint.write_reg(8, 4U);
            const bool armed = bound && irq.arm_irq();
            ctl.write<reg::uart_fault>(1U << direction);
            barrier();
            const int event = armed ? irq.wait_for_irq(std::chrono::milliseconds(10)) : -1;
            const auto errors = endpoint.read_reg(24), status = endpoint.read_reg(4);
            ctl.write<reg::uart_fault>(0);
            barrier();
            add("uart", "framing_break_" + std::to_string(direction),
                event > 0 && errors > 0 && (status & 255) == 0, errors, "detected frames",
                "forced low stop bit must raise error IRQ and must not enqueue a corrupt byte");
        };
        Uio<mem::uart_a> irq_a;
        Uio<mem::uart_b> irq_b;
        framing(rx, irq_b, 0);
        framing(tx, irq_a, 1);
        a.reset();
        b.reset();
        // Bounded batches avoid overflowing TX; deliberately never drain RX.
        for (unsigned batch = 0; batch < 3; ++batch)
        {
            for (unsigned i = 0; i < 64; ++i)
                tx.write_reg(0, uint32_t(i));
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        const auto overrun = rx.read_reg(24);
        add("uart", "detect_rx_overrun", overrun > 0, overrun, "errors detected",
            "deliberately omit RX servicing beyond the 64-byte FIFO");
        a.reset();
        b.reset();
        for (unsigned i = 0; i < 256; ++i)
            tx.write_reg(0, i);
        barrier();
        const auto tx_overrun = tx.read_reg(24);
        add("uart", "detect_tx_overrun", tx_overrun > 0, tx_overrun, "errors detected",
            "256 immediate writes exceed the 64-byte TX FIFO; overflow must be visible");
        a.reset();
        b.reset();
        ar = {};
        br = {};
        StartGate recovery_gate{2};
        std::thread recover_a([&] { ar = a.exchange(256, 0, false, &recovery_gate); }),
            recover_b([&] { br = b.exchange(256, 1, false, &recovery_gate); });
        recover_a.join();
        recover_b.join();
        add("uart", "overrun_recovery", !ar.errors && !br.errors, double(ar.errors + br.errors),
            "errors", "reset clears overrun and both directions recover byte-exactly");
    }
    void analog(unsigned mask)
    {
        for (unsigned channel = 0; channel < 2; ++channel)
        {
            if (!(mask & (1U << channel)))
            {
                add("analog", "channel_" + std::to_string(channel), true, 0, "connected cables",
                    "external DAC-to-ADC cable absent", "", true);
                continue;
            }
            for (auto [bin, amplitude, dc, decimation] :
                 std::array<std::tuple<unsigned, int, int, unsigned>, 7>{{{8, 512, 0, 8},
                                                                          {8, 2048, 0, 8},
                                                                          {8, 4096, 0, 8},
                                                                          {32, 4096, 0, 8},
                                                                          {64, 2048, 0, 4},
                                                                          {8, 2048, 500, 8},
                                                                          {8, 2048, -500, 16}}})
            {
                if (!available())
                    return;
                dma.reset();
                for (unsigned i = 0; i < 1024; ++i)
                {
                    const int code = int(std::lround(
                        amplitude * std::cos(2 * std::numbers::pi * bin * i / 1024) + dc));
                    const uint32_t word = uint32_t(code) & 0x3fff;
                    // Keep the other DAC quiet so a crossed cable cannot pass.
                    bram.write_reg(4 * i, word << (16 * channel));
                }
                barrier();
                ctl.write<reg::mode>(2);
                ctl.write<reg::decimation>(decimation);
                ctl.write<reg::samples>(4096);
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                const auto r = dma.transfer({16384}, 0, 0, false, true, true);
                snapshot();
                double mean = 0, cosine = 0, sine = 0, power = 0;
                unsigned clipped = 0;
                for (unsigned i = 0; i < 4096; ++i)
                {
                    const double value =
                        adc_signed(ram.read_reg(bist_full::Dma::rx_base + 4 * i), channel);
                    const double phase = 2 * std::numbers::pi * bin * decimation * i / 1024;
                    mean += value;
                    cosine += value * std::cos(phase);
                    sine += value * std::sin(phase);
                    power += value * value;
                    clipped += std::abs(value) >= 8190;
                }
                mean /= 4096;
                cosine *= 2.0 / 4096;
                sine *= 2.0 / 4096;
                power /= 4096;
                const double peak = std::hypot(cosine, sine),
                             residual =
                                 std::sqrt(std::max(0.0, power - mean * mean - peak * peak / 2));
                const bool ok = r.ok && !sts.read<reg::overflow>() && peak > amplitude * .4 &&
                                peak < amplitude * 1.6 && residual < peak * .12 + 10 &&
                                std::abs(mean - dc) < 350 && clipped == 0;
                add("analog",
                    "ch" + std::to_string(channel) + "_bin" + std::to_string(bin) + "_amp" +
                        std::to_string(amplitude) + "_dc" + std::to_string(dc),
                    ok, peak, "ADC codes peak",
                    "uncalibrated frequency/amplitude/DC sweep, exact DMA length, no FIFO loss or "
                    "clipping",
                    "\"frequency_hz\":" + number(125000000.0 * bin / 1024) +
                        ",\"decimation\":" + std::to_string(decimation) +
                        ",\"mean\":" + number(mean) + ",\"residual_rms\":" + number(residual));
            }
        }
        dma.reset();
        ctl.write<reg::mode>(2);
        ctl.write<reg::samples>(4096);
        ctl.write<reg::decimation>(1);
        ctl.write<reg::capture>(ctl.read<reg::capture>() + 1);
        barrier();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        snapshot();
        const auto lost = sts.read<reg::overflow>();
        add("analog", "detect_capture_overflow", lost > 0, lost, "lost sample slots",
            "capture with S2MM stopped must expose backpressure/sample loss");
        recover("capture_overflow");
    }
    void stress()
    {
        dma.reset();
        ctl.write<reg::scratch>(seed);
        ctl.write<reg::packet_bytes>(65536);
        dma.fill(65536, seed, 0, 0);
        SerialEndpoint<mem::uart_a> a(cancelled);
        SerialEndpoint<mem::uart_b> b(cancelled);
        const bool opened = a.open() && b.open();
        a.reset();
        b.reset();
        std::atomic<bool> done{false};
        StartGate gate{5};
        std::atomic<int> affinity_error{0};
        uint64_t checksum = 0;
        std::thread cpu(
            [&]
            {
                cpu_set_t cpu;
                CPU_ZERO(&cpu);
                CPU_SET(1, &cpu);
                affinity_error = pthread_setaffinity_np(pthread_self(), sizeof(cpu), &cpu);
                std::vector<uint32_t> memory(2 * 1024 * 1024, seed);
                gate.wait();
                while (!done && !cancelled)
                {
                    for (auto &x : memory)
                    {
                        x = x * 1664525U + 1013904223U;
                        checksum += x;
                    }
                }
            });
        uint64_t ddr_bytes = 0, ddr_errors = 0;
        std::thread memory_master(
            [&]
            {
                bist_full::pin(1);
                gate.wait();
                while (!done && available())
                {
                    const auto w =
                        ddr.run(32 * 1024 * 1024, 4 * 1024 * 1024, seed, 3, true, deadline);
                    const auto r =
                        ddr.run(32 * 1024 * 1024, 4 * 1024 * 1024, seed, 3, false, deadline);
                    ddr_errors += !w.ok() || !r.ok();
                    if (w.ok() && r.ok())
                        ddr_bytes += w.bytes + r.bytes;
                }
            });
        SerialResult ar, br;
        std::thread serial_a(
            [&]
            {
                if (opened)
                    ar = a.exchange(131072, 0, false, &gate);
                else
                    gate.drop();
            }),
            serial_b(
                [&]
                {
                    if (opened)
                        br = b.exchange(131072, 1, false, &gate);
                    else
                        gate.drop();
                });
        cpu_set_t cpu_affinity;
        CPU_ZERO(&cpu_affinity);
        CPU_SET(0, &cpu_affinity);
        const int dma_affinity_error =
            pthread_setaffinity_np(pthread_self(), sizeof(cpu_affinity), &cpu_affinity);
        uint64_t errors = 0, bytes = 0;
        gate.wait();
        const auto begin = Clock::now();
        while (seconds(begin) < 3 && available())
        {
            ctl.write<reg::reset_diag>(ctl.read<reg::reset_diag>() + 1);
            barrier();
            auto r = dma.transfer({65536});
            errors += !r.ok;
            errors += dma.verify(65536, seed, 0);
            bytes += r.ok ? 65536 : 0;
        }
        serial_a.join();
        serial_b.join();
        done = true;
        cpu.join();
        memory_master.join();
        add("stress", "cpu_dma_uart",
            opened && !errors && !ar.errors && !br.errors && !affinity_error &&
                !dma_affinity_error && bytes > 0 && !ddr_errors && ddr_bytes > 0,
            bytes / seconds(begin) / 1e6, "MB/s verified DMA",
            "CPU memory churn, HP0 DMA, independent HP1 DDR writer/checker and duplex UART; "
            "integrity "
            "retained",
            "\"ddr_bytes\":" + std::to_string(ddr_bytes) + ",\"ddr_errors\":" +
                std::to_string(ddr_errors) + ",\"checksum\":" + quote(std::to_string(checksum)) +
                ",\"dma_errors\":" + std::to_string(errors) +
                ",\"uart_errors\":" + std::to_string(ar.errors + br.errors) +
                ",\"uart_a_received\":" + std::to_string(ar.received) +
                ",\"uart_b_received\":" + std::to_string(br.received));
    }
    void run(unsigned mask)
    {
        platform();
        if (available())
        {
            group_done("platform");
            registers();
        }
        if (available())
        {
            group_done("memory");
            dma.stop();
            if (reservation_ok)
                ddr.exercise(seed, deadline, output());
            else
                add("ddr", "reservation_required", false, 0, "bytes",
                    "DDR master disabled: boot reservation did not validate");
        }
        if (available())
        {
            group_done("ddr");
            bist_full::processors(seed, cancelled, deadline, output());
            group_done("cpu");
            bist_full::board_identity(output());
            group_done("identity");
            bist_full::storage(prefix, seed, output());
            group_done("storage");
        }
        if (available())
        {
            dma_matrix();
        }
        if (available())
        {
            faults();
            group_done("dma");
            interrupts();
        }
        if (available())
        {
            group_done("irq");
            serial();
        }
        if (available())
        {
            group_done("uart");
            analog(mask);
            if (available())
                bist_full::Analog(dma, cancelled, deadline, output(), prefix).exercise(mask);
        }
        if (available())
        {
            group_done("analog");
            stress();
        }
        if (available())
        {
            group_done("stress");
            const auto retained =
                ddr.run(16 * 1024 * 1024, 8 * 1024 * 1024, seed, 3, false, deadline);
            add("ddr", "retention_after_load", retained.ok() && retained.bytes == 8 * 1024 * 1024,
                retained.mismatches, "errors",
                "DDR pattern retained across CPU, media, UART, converter and DMA phases",
                retained.json());
        }
        ddr.stop();
        ctl.write<reg::uart_fault>(0);
        const bool within = available();
        dma.stop();
        add("lifecycle", "onboard_budget", within, seconds(beginning), "s",
            "onboard allowance leaves time inside the 60 s end-to-end budget");
        {
            std::lock_guard lock(mutex);
            state = !reservation_ok ? "unsafe_fixture"
                    : cancelled     ? "cancelled"
                    : within        ? "complete"
                                    : "timeout";
            persist();
            active = false;
        }
        completed.notify_all();
    }
};
BistFull::BistFull() : impl(std::make_unique<Impl>()) {}
BistFull::~BistFull() = default;
bool BistFull::start(uint32_t mask, uint32_t budget_ms, uint32_t seed)
{
    if (mask > 3 || budget_ms < 1000 || budget_ms > 35000 || impl->active)
        return false;
    if (impl->worker.joinable())
        impl->worker.join();
    std::lock_guard lock(impl->mutex);
    std::error_code error;
    std::filesystem::create_directories(root, error);
    if (error)
        return false;
    impl->run_id = std::to_string(std::chrono::duration_cast<std::chrono::microseconds>(
                                      std::chrono::system_clock::now().time_since_epoch())
                                      .count());
    impl->prefix = std::string(root) + "/" + impl->run_id;
    const bool created = std::filesystem::create_directory(impl->prefix, error);
    if (!created || error)
        return false;
    impl->storage_ok = true;
    impl->checks = "[]";
    impl->groups = "[]";
    impl->host.clear();
    impl->log.clear();
    impl->state = "running";
    impl->seed = seed;
    impl->observers = 0;
    utsname system{};
    uname(&system);
    impl->metadata =
        "{\"instrument_version\":\"0.2.0\",\"workload_version\":2,\"source_sha256\":" +
        quote(FULL_BUILD_ID) + ",\"sdk_version\":" + quote(KOHERON_VERSION) +
        ",\"board\":\"STEMlab 125-14 Gen1\",\"kernel\":" + quote(system.release) +
        ",\"os\":" + fields_json(release_fields(read_file("/etc/os-release"))) +
        ",\"os_image\":" + fields_json(release_fields(read_file("/etc/koheron-release"))) +
        ",\"instrument_manifest\":" + quote(read_file("/tmp/live-instrument/instrument.json")) +
        ",\"fpga_sha256\":" + quote(digest("/tmp/live-instrument/bist-full.bit.bin")) +
        ",\"server_sha256\":" + quote(digest("/proc/" + std::to_string(getpid()) + "/exe")) +
        ",\"boot_id\":" + quote(read_file("/proc/sys/kernel/random/boot_id")) + ",\"fpga_dna\":" +
        quote(std::to_string(impl->sts.read<reg::dna0>() |
                             (uint64_t(impl->sts.read<reg::dna1>()) << 32))) +
        ",\"parameters\":{\"analog_mask\":" + std::to_string(mask) +
        ",\"seed\":" + std::to_string(seed) +
        ",\"onboard_budget_ms\":" + std::to_string(budget_ms) +
        ",\"end_to_end_budget_seconds\":60,\"dma_dre\":true,\"dma_maximum_bytes\":2097152,\"bram_"
        "bytes\":16384,\"ddr_test_bytes\":134217728,\"cacheable_test_bytes\":33554432}}";
    impl->write(std::string(root) + "/latest", impl->run_id);
    impl->cancelled = false;
    impl->reservation_ok = false;
    impl->active = true;
    impl->beginning = Clock::now();
    impl->deadline = impl->beginning + std::chrono::milliseconds(budget_ms);
    impl->persist();
    impl->worker = std::thread([this, mask] { impl->run(mask); });
    return true;
}
void BistFull::cancel() { impl->cancelled = true; }
std::string BistFull::get_report()
{
    if (impl->active)
        ++impl->observers;
    std::lock_guard lock(impl->mutex);
    return impl->report();
}
std::string BistFull::get_artifact(std::string run_id, std::string name)
{
    std::lock_guard lock(impl->mutex);
    if (impl->active || run_id != impl->run_id || !name.starts_with("ch") ||
        !name.ends_with(".json") || name.size() > 80 ||
        name.substr(0, name.size() - 5)
                .find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_-") != std::string::npos)
        return "";
    return read_file(impl->prefix + '/' + name);
}
uint32_t BistFull::ping(uint32_t value) { return value ^ 0x42494631U; }
uint64_t BistFull::echo_u64(uint64_t value) { return value; }
double BistFull::echo_double(double value) { return value; }
std::vector<uint32_t> BistFull::echo_vector(std::vector<uint32_t> values) { return values; }
bool BistFull::record_host_results(std::string run_id, std::string results_json)
{
    std::lock_guard lock(impl->mutex);
    if (impl->active || run_id != impl->run_id || results_json.size() > 65536)
        return false;
    impl->host = std::move(results_json);
    impl->persist();
    return impl->storage_ok;
}
bool BistFull::wait(uint32_t timeout_ms)
{
    std::unique_lock lock(impl->mutex);
    const auto id = impl->run_id;
    const auto end = Clock::now() + std::chrono::milliseconds(std::min(timeout_ms, 60000U));
    const auto done = [&] { return !impl->active || impl->run_id != id; };
    while (!done() && Clock::now() < end && !rt::SignalHandler::s_interrupted)
        impl->completed.wait_until(
            lock, std::min(end, Clock::now() + std::chrono::milliseconds(100)), done);
    return done();
}
bool BistFull::recover_devices()
{
    if (impl->active)
        return false;
    if (impl->worker.joinable())
        impl->worker.join();
    impl->cancelled = false;
    if (!impl->reservation_valid())
        return false;
    const uint32_t key = 0x7265636f;
    bool ok = impl->dma.open() && impl->dma.reset();
    impl->ctl.write<reg::scratch>(key);
    impl->ctl.write<reg::packet_bytes>(4096);
    impl->dma.fill(4096, key, 0, 0);
    const auto transfer = impl->dma.transfer({4096});
    ok &= transfer.ok && !impl->dma.verify(4096, key, 0) && !impl->dma.guards(4096, 0, 0);
    SerialEndpoint<mem::uart_a> a(impl->cancelled);
    SerialEndpoint<mem::uart_b> b(impl->cancelled);
    if (!a.open() || !b.open())
        return false;
    a.reset();
    b.reset();
    StartGate gate{2};
    SerialResult ar, br;
    std::thread first([&] { ar = a.exchange(256, 0, false, &gate); });
    std::thread second([&] { br = b.exchange(256, 1, false, &gate); });
    first.join();
    second.join();
    const auto end = Clock::now() + std::chrono::seconds(1);
    const auto written = impl->ddr.run(64 * 1024 * 1024, 65536, key, 3, true, end);
    const auto checked = impl->ddr.run(64 * 1024 * 1024, 65536, key, 3, false, end);
    ok &= written.ok() && checked.ok() && written.bytes == 65536 && checked.bytes == 65536;
    impl->ddr.stop();
    impl->dma.stop();
    return ok && !ar.errors && !br.errors && ar.interrupts > 0 && br.interrupts > 0;
}
bool FullCompletion::wait(uint32_t timeout_ms)
{
    return rt::get_driver<BistFull>().wait(timeout_ms);
}
