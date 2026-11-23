#pragma once
#include "../bist/hardware.hpp"
#include <functional>

namespace bist_full
{
using Emit = std::function<void(const std::string &, const std::string &, bool, double,
                                const std::string &, const std::string &, const std::string &)>;
struct DdrResult
{
    bool completed = false;
    uint32_t errors = 0, mismatches = 0, first = 0, bursts = 0;
    uint64_t bytes = 0, cycles = 0, expected = 0, actual = 0;
    bool ok() const { return completed && !errors && !mismatches; }
    std::string json() const
    {
        return "\"transferred_bytes\":" + std::to_string(bytes) +
               ",\"axi_errors\":" + std::to_string(errors) +
               ",\"mismatches\":" + std::to_string(mismatches) +
               ",\"first_address\":" + std::to_string(first) +
               ",\"first_expected\":" + bist::quote(std::to_string(expected)) +
               ",\"first_actual\":" + bist::quote(std::to_string(actual)) +
               ",\"bursts\":" + std::to_string(bursts) + ",\"cycles\":" + std::to_string(cycles);
    }
};
class Ddr
{
    hw::Memory<mem::control> &ctl = hw::get_memory<mem::control>();
    hw::Memory<mem::status> &sts = hw::get_memory<mem::status>();
    hw::Memory<mem::buffers> &ram = hw::get_memory<mem::buffers>();
    std::atomic<bool> &cancel;
    static uint32_t scramble(uint32_t x)
    {
        x ^= x << 13;
        x ^= x >> 17;
        return x ^ (x << 5);
    }

  public:
    explicit Ddr(std::atomic<bool> &stop) : cancel(stop) {}
    static uint64_t pattern(uint32_t address, uint32_t key, unsigned kind, bool invert = false)
    {
        uint64_t p = 0;
        switch (kind)
        {
        case 0:
            p = (uint64_t(~address ^ key) << 32) | (address ^ ((key << 16) | (key >> 16)));
            break;
        case 1:
            p = (uint64_t(key) << 32) | key;
            break;
        case 2:
            p = uint64_t(1) << ((address >> 3) & 63);
            break;
        default:
            break;
        case 3:
            p = (uint64_t(scramble(address ^ key ^ 0x9e3779b9U)) << 32) | scramble(address ^ key);
            break;
        }
        return invert ? ~p : p;
    }
    bool stop()
    {
        ctl.write<reg::ddr_abort>(1);
        bist::barrier();
        const auto end = bist::Clock::now() + std::chrono::milliseconds(100);
        while (sts.read<reg::ddr_busy>() && bist::Clock::now() < end)
            std::this_thread::yield();
        return !sts.read<reg::ddr_busy>();
    }
    DdrResult run(uint32_t offset, uint32_t n, uint32_t key, unsigned kind, bool write,
                  bist::Clock::time_point deadline, unsigned burst = 256, uint32_t pauses = 0,
                  bool invert = false, unsigned strobe = 255)
    {
        if (sts.read<reg::ddr_busy>())
            return {};
        ctl.write<reg::ddr_abort>(0);
        ctl.write<reg::ddr_config>((strobe << 8) | (kind << 4) | (invert ? 4 : 0) |
                                   (write ? 1 : 2));
        ctl.write<reg::ddr_address>(0x18000000U + offset);
        ctl.write<reg::ddr_bytes>(n);
        ctl.write<reg::ddr_seed>(key);
        ctl.write<reg::ddr_burst>(burst);
        ctl.write<reg::ddr_pauses>(pauses);
        const auto token = ctl.read<reg::ddr_command>() + 1;
        bist::barrier();
        ctl.write<reg::ddr_command>(token);
        const auto end = std::min(deadline, bist::Clock::now() + std::chrono::seconds(4));
        while (sts.read<reg::ddr_done>() != token && !cancel && bist::Clock::now() < end)
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        const bool completed = sts.read<reg::ddr_done>() == token;
        if (!completed)
            stop();
        bist::barrier();
        return {completed,
                sts.read<reg::ddr_errors>(),
                sts.read<reg::ddr_mismatches>(),
                sts.read<reg::ddr_first_address>(),
                sts.read<reg::ddr_bursts>(),
                sts.read<reg::ddr_transferred0>() |
                    (uint64_t(sts.read<reg::ddr_transferred1>()) << 32),
                sts.read<reg::ddr_cycles0>() | (uint64_t(sts.read<reg::ddr_cycles1>()) << 32),
                sts.read<reg::ddr_expected0>() | (uint64_t(sts.read<reg::ddr_expected1>()) << 32),
                sts.read<reg::ddr_actual0>() | (uint64_t(sts.read<reg::ddr_actual1>()) << 32)};
    }
    void exercise(uint32_t seed, bist::Clock::time_point deadline, const Emit &emit)
    {
        auto available = [&] { return !cancel && bist::Clock::now() < deadline; };
        auto add =
            [&](const std::string &name, const DdrResult &r, bool pass, const std::string &detail)
        {
            emit("ddr", name, pass, r.cycles ? double(r.bytes) * 125 / double(r.cycles) : 0, "MB/s",
                 detail, r.json());
        };
        constexpr uint32_t size = mem::buffers_range;
        for (auto [name, kind, key, invert] :
             std::array<std::tuple<const char *, unsigned, uint32_t, bool>, 7>{
                 {{"zeros", 1, 0, false},
                  {"ones", 1, ~0U, false},
                  {"checkerboard", 1, 0x55aa55aaU, false},
                  {"address", 0, seed, false},
                  {"walking_one", 2, 0, false},
                  {"walking_zero", 2, 0, true},
                  {"scrambled", 3, seed, false}}})
        {
            if (!available())
                return;
            const auto w = run(0, size, key, kind, true, deadline, 256, 0, invert);
            add(std::string(name) + "_write_128m", w, w.ok() && w.bytes == size,
                "independent HP1 master writes every reserved DDR byte");
            const auto r = run(0, size, key, kind, false, deadline, 256, 0, invert);
            add(std::string(name) + "_read_128m", r, r.ok() && r.bytes == size,
                "hardware pattern checker compares every 64-bit beat; no CPU copy or DMA");
            if (!r.completed || !available())
                return;
            uint64_t errors = 0;
            // Every 4 KiB page plus the final beat, independently calculated on the CPU.
            for (uint32_t offset = 0; offset < size; offset += 4096)
                errors += ram.read_reg<uint64_t>(offset) !=
                          pattern(0x18000000U + offset, key, kind, invert);
            errors += ram.read_reg<uint64_t>(size - 8) !=
                      pattern(0x18000000U + size - 8, key, kind, invert);
            emit("ddr", std::string(name) + "_cpu_crosscheck", !errors, double(errors), "errors",
                 "CPU independently checks one beat in every DDR page and the final beat", "");
        }
        // CPU-originated writes at the far end exercise a different producer and all data lanes.
        constexpr uint32_t far = size - 65536;
        for (uint32_t o = far; o < size; o += 8)
            ram.write_reg<uint64_t>(o, pattern(0x18000000U + o, seed, 0));
        bist::barrier();
        auto r = run(far, 65536, seed, 0, false, deadline);
        add("cpu_write_pl_read", r, r.ok() && r.bytes == 65536,
            "CPU produces; independent PL checker consumes the final 64 KiB");
        const uint32_t bad = far + 136;
        const auto expected = pattern(0x18000000U + bad, seed, 0);
        ram.write_reg<uint64_t>(bad, expected ^ (uint64_t(1) << 41));
        bist::barrier();
        r = run(far, 65536, seed, 0, false, deadline);
        add("detect_cpu_corruption", r,
            r.completed && !r.errors && r.mismatches == 1 && r.first == 0x18000000U + bad &&
                r.expected == expected && r.actual == (expected ^ (uint64_t(1) << 41)),
            "one CPU-flipped bit must yield exactly one error with exact first address/values");
        ram.write_reg<uint64_t>(bad, expected);
        bist::barrier();
        r = run(far, 65536, seed, 0, false, deadline);
        add("corruption_recovery", r, r.ok(),
            "restoring the byte clears the next run's diagnostics");
        for (unsigned burst : {1U, 16U, 64U, 256U})
        {
            if (!available())
                return;
            const auto w = run(far + 4088, 16392, seed, 3, true, deadline, burst, 0x80);
            r = run(far + 4088, 16392, seed, 3, false, deadline, burst, 0x8000);
            add("burst_" + std::to_string(burst) + "_4k_crossing", r,
                w.ok() && r.ok() && w.bytes == 16392 && r.bytes == 16392,
                "burst splitting from 8 bytes before a 4 KiB boundary with write/read pauses");
        }
        run(far, 512, 0, 1, true, deadline);
        run(far, 512, seed, 0, true, deadline, 16, 0, false, 0x55);
        uint64_t errors = 0;
        for (unsigned i = 0; i < 64; ++i)
            errors += ram.read_reg<uint64_t>(far + 8 * i) !=
                      (pattern(0x18000000U + far + 8 * i, seed, 0) & 0x00ff00ff00ff00ffULL);
        emit("ddr", "write_strobe_isolation", !errors, double(errors), "errors",
             "alternating byte lanes change; all four unselected lanes retain zero", "");
        run(far, 512, seed, 0, true, deadline, 16, 0, false, 0xaa);
        r = run(far, 512, seed, 0, false, deadline);
        add("complementary_write_strobes", r, r.ok(),
            "complementary writes reconstruct the full expected word");
        // Invalid requests reach the RTL, which must reject them without any bus transaction.
        errors = 0;
        for (auto [offset, n, burst] :
             std::array<std::tuple<uint32_t, uint32_t, unsigned>, 6>{{{0xfffffff8U, 16, 16},
                                                                      {size - 8, 16, 16},
                                                                      {0, 0, 16},
                                                                      {1, 8, 16},
                                                                      {0, 7, 16},
                                                                      {0, 8, 257}}})
        {
            r = run(offset, n, seed, 0, true, deadline, burst);
            errors += !r.completed || r.errors != 1 || r.bytes != 0 || r.bursts != 0;
        }
        emit("ddr", "hardware_address_fence", !errors, double(errors), "errors",
             "RTL rejects lower/upper boundary escapes, misalignment, zero/nonbeat length and "
             "illegal bursts",
             "");
        if (available())
        {
            r = run(far, 65536, seed, 0, true,
                    std::min(deadline, bist::Clock::now() + std::chrono::microseconds(200)), 256,
                    0xf0);
            const bool aborted = !r.completed && !sts.read<reg::ddr_busy>() && r.bytes > 0 &&
                                 r.bytes < 65536 && !r.errors;
            add("draining_abort", r, aborted,
                "abort under write throttling drains the active burst and leaves no AXI "
                "transaction outstanding");
            r = run(far, 512, seed, 0, true, deadline);
            const auto checked = run(far, 512, seed, 0, false, deadline);
            add("abort_recovery", checked, r.ok() && checked.ok(),
                "fresh write and check after orderly abort");
        }
        // Full-region retention after other work is tested in a later phase.
        r = run(16 * 1024 * 1024, 8 * 1024 * 1024, seed, 3, true, deadline);
        add("retention_prepare", r, r.ok(),
            "8 MiB outside DMA buffers retained across subsequent instrument phases");
    }
};
} // namespace bist_full
