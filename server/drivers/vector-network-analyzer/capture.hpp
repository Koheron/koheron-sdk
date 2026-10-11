#ifndef KOHERON_VNA_CAPTURE_HPP
#define KOHERON_VNA_CAPTURE_HPP
#include "server/hardware/memory_manager.hpp"
#include "server/runtime/driver_manager.hpp"
#include "sweep-plan.hpp"
#include <array>
#include <chrono>
#include <mutex>
#include <span>
#include <thread>
#include <tuple>
#include <vector>

// Checked one-shot DMA. A token and renewable lease prevent overlapping clients
// from reseeding a capture or reading a mixture of two acquisition records.
template<class ClockPolicy> class VNACapture {
  public:
    VNACapture() {
        sclr.write<0x8>(0xDF0D);
        // Map the descriptor bank high without remapping the other OCM banks
        // over Linux's low DDR/vector memory. Preserve the boot configuration.
        sclr.set_bit<0x910, 3>();
        sclr.clear_bit<0x8000, 0>(); sclr.clear_bit<0x8014, 0>();
        ctl.write<reg::acq_reset>(1);
        ctl.write<reg::output_gain>(0);
    }
    ~VNACapture() { halt(); }

    // Error: 0 success, 1 invalid settings, 2 another capture owns DMA,
    // 3 reset timeout. Token zero always means no acquisition was started.
    std::tuple<uint32_t, uint32_t> begin_sweep(double start_hz, double stop_hz,
                                            double duration, double level_dbfs,
                                            uint32_t samples = vna::capture_samples) {
        std::lock_guard lock(mutex);
        vna::SweepPlan next;
        if (!vna::make_plan(start_hz, stop_hz, duration, level_dbfs, next, ClockPolicy::rate) ||
            !vna::valid_record(samples, next.samples)) return {0, 1};
        if (owned && std::chrono::steady_clock::now() >= deadline) halt();
        if (owned) return {0, 2};
        std::lock_guard clock_lock(ClockPolicy::mutex());
        ClockPolicy::ensure();
        halt();
        dma.write<0x30>(4);
        for (unsigned i = 0; i < 100 && (dma.read<0x30>() & 4); ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if (dma.read<0x30>() & 4) return {0, 3};
        plan = next;
        record_samples = samples;
        n_desc = samples * 2 / packet_bytes;
        ctl.write<reg::trigger>(0); ctl.write<reg::seed_write>(0);
        ctl.write<reg::chirp_samples>(plan.samples);
        ctl.write<reg::capture_samples>(samples);
        ctl.write<reg::output_gain>(plan.gain);
        ctl.write<reg::chirp_coefficient0>(uint32_t(plan.coefficient));
        ctl.write<reg::chirp_coefficient1>(uint32_t(plan.coefficient >> 32));
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        ctl.write<reg::acq_reset>(0);
        for (uint32_t i = 0; i < 16; ++i) {
            ctl.write<reg::seed_index>(i);
            ctl.write<reg::seed_data0>(uint32_t(plan.seeds[i]));
            ctl.write<reg::seed_data1>(uint32_t(plan.seeds[i] >> 32));
            ctl.write<reg::seed_write>(1); ctl.write<reg::seed_write>(0);
        }
        for (uint32_t i = 0; i < n_desc; ++i) {
            for (uint32_t j = 0; j < 64; j += 4) ocm.write_reg(64 * i + j, 0U);
            ocm.write_reg(64 * i, mem::ocm_s2mm_addr + 64 * ((i + 1) % n_desc));
            ocm.write_reg(64 * i + 8, mem::ram_s2mm_addr + packet_bytes * i);
            ocm.write_reg(64 * i + 24, packet_bytes);
        }
        flush();
        if (++sequence == 0) ++sequence;
        owned = true; completed = false;
        capture_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        renew();
        dma.write<0x38>(mem::ocm_s2mm_addr);
        dma.write<0x30>(1);
        dma.write<0x40>(mem::ocm_s2mm_addr + (n_desc - 1) * 64);
        flush();
        ctl.write<reg::trigger>(1); ctl.write<reg::trigger>(0);
        return {sequence, 0};
    }

    // Exact words used by the FPGA, also used by the browser's reference model.
    auto get_plan(uint32_t token) {
        std::lock_guard lock(mutex);
        std::array<uint32_t, 36> words{};
        if (!accept(token)) return words;
        words[0] = plan.samples; words[1] = plan.gain;
        words[2] = uint32_t(plan.coefficient); words[3] = uint32_t(plan.coefficient >> 32);
        for (unsigned i = 0; i < 16; ++i) {
            words[4 + 2*i] = uint32_t(plan.seeds[i]);
            words[5 + 2*i] = uint32_t(plan.seeds[i] >> 32);
        }
        return words;
    }

    // 0 running, 1 complete, 2 overflow, 3 near-rail ADC, 4 DMA error,
    // 5 expired/stale token, 6 capture timeout, 7 clock changed.
    uint32_t get_status(uint32_t token) {
        std::lock_guard lock(mutex);
        if (!accept(token)) return 5;
        std::lock_guard clock_lock(ClockPolicy::mutex());
        if (!ClockPolicy::matches()) { halt(); return 7; }
        if (completed) return 1;
        const auto status = sts.read<reg::capture_status>();
        if (status & 4) { halt(); return 2; }
        if (dma.read<0x34>() & 0x770) { halt(); return 4; }
        if (!(status & 2) || !(ocm.read_reg((n_desc - 1) * 64 + 28) & 0x80000000U)) {
            if (std::chrono::steady_clock::now() > capture_deadline) { halt(); return 6; }
            return 0;
        }
        flush();
        for (uint32_t i = 0; i < n_desc; ++i) {
            const auto descriptor = ocm.read_reg(64 * i + 28);
            if ((descriptor & 0xF0000000U) != 0x80000000U ||
                (descriptor & 0x7FFFFFU) != packet_bytes) { halt(); return 4; }
        }
        if (status & 8) { halt(); return 3; }
        completed = true;
        return 1;
    }

    std::vector<uint32_t> get_adc_block(uint32_t token, uint32_t index) {
        return get_adc_blocks(token, index, 1);
    }
    std::vector<uint32_t> get_adc_blocks(uint32_t token, uint32_t index, uint32_t count) {
        std::lock_guard lock(mutex);
        if (!accept(token) || !completed || count == 0 || count > vna::max_batch_blocks ||
            index >= n_desc || count > n_desc - index) return {};
        const auto block = ram.read_span<uint32_t>(record_samples / 2)
            .subspan(index * packet_bytes / 4, count * packet_bytes / 4);
        return {block.begin(), block.end()};
    }
    uint32_t get_record_samples(uint32_t token) {
        std::lock_guard lock(mutex);
        return accept(token) ? record_samples : 0;
    }
    // Releases the lease after download, or cancels an acquisition. A stale
    // client's cancellation cannot stop a later client's sweep.
    bool release(uint32_t token) {
        std::lock_guard lock(mutex);
        if (!owned || token != sequence) return false;
        halt(); return true;
    }

  private:
    static constexpr uint32_t packet_bytes = vna::packet_bytes;
    uint32_t n_desc = 512, record_samples = vna::capture_samples;
    std::mutex mutex;
    uint32_t sequence = 0;
    bool owned = false, completed = false;
    vna::SweepPlan plan;
    std::chrono::steady_clock::time_point deadline, capture_deadline;
    hw::Memory<mem::control>& ctl = hw::get_memory<mem::control>();
    hw::Memory<mem::status>& sts = hw::get_memory<mem::status>();
    hw::Memory<mem::dma>& dma = hw::get_memory<mem::dma>();
    hw::Memory<mem::ram_s2mm>& ram = hw::get_memory<mem::ram_s2mm>();
    hw::Memory<mem::ocm_s2mm>& ocm = hw::get_memory<mem::ocm_s2mm>();
    hw::Memory<mem::sclr>& sclr = hw::get_memory<mem::sclr>();
    void renew() { deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30); }
    bool accept(uint32_t token) {
        if (!owned || token != sequence) return false;
        if (std::chrono::steady_clock::now() >= deadline) { halt(); return false; }
        renew(); return true;
    }
    void halt() {
        ctl.write<reg::acq_reset>(1); ctl.write<reg::output_gain>(0);
        dma.write<0x30>(0); owned = false; completed = false;
    }
    void flush() { (void)dma.read<0x34>(); asm volatile("dsb sy" ::: "memory"); }
};
#endif
