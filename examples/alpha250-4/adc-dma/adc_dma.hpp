#ifndef ALPHA250_4_ADC_DMA_HPP
#define ALPHA250_4_ADC_DMA_HPP

#include "server/hardware/memory_manager.hpp"
#include "server/runtime/driver_manager.hpp"
#include "boards/alpha250-4/drivers/clock-generator.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <span>
#include <thread>

// Two finite SG chains: 64 MiB per ADC pair, up to 256 descriptors each.
class AdcDma {
  public:
    AdcDma() {
        auto& slcr = hw::get_memory<mem::sclr>();
        slcr.write<0x8>(0xDF0D);
        // Put only bank 3 high, preserving the kernel SRAM bank's mapping.
        slcr.write<0x910>(slcr.read<0x910>() | 0b1000);
        auto& hp0 = hw::get_memory<mem::axi_hp0>();
        auto& hp2 = hw::get_memory<mem::axi_hp2>();
        hp0.clear_bit<0x0, 0>();
        hp0.clear_bit<0x14, 0>();
        hp2.clear_bit<0x0, 0>();
        hp2.clear_bit<0x14, 0>();
        stop();
    }

    std::array<uint32_t, 3> get_info() {
        return {sample_rate, max_samples, packet_samples};
    }

    // Only even sample counts: two sample times in each 64-bit DMA beat.
    bool configure(uint32_t samples, bool test_pattern) {
        stop();
        if (samples < 2 || samples > max_samples || (samples & 1)) return false;
        const auto frequencies = rt::get_driver<ClockGenerator>().get_adc_sampling_freq();
        if (!(std::abs(frequencies[0] - sample_rate) < 0.5) ||
            !(std::abs(frequencies[1] - sample_rate) < 0.5)) return false;
        if (!reset_dmas()) return false;
        n_samples = samples;
        n_desc = (samples + packet_samples - 1) / packet_samples;
        prepare<mem::ocm0, mem::ram0_addr, mem::ocm0_addr>();
        prepare<mem::ocm1, mem::ram1_addr, mem::ocm1_addr>();
        ctl.write<reg::sample_count>(samples);
        ctl.write<reg::test_pattern>(test_pattern ? 1 : 0);
        barrier();
        // Flush the ADC FIFO and both sides of each CDC before releasing reset.
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        ctl.write<reg::acq_reset>(0);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if (!clock_locked()) {
            stop();
            return false;
        }
        state = State::ready;
        return true;
    }

    bool arm() {
        if (state != State::ready || !clock_locked()) return false;
        state = State::idle;
        arm_chain<mem::dma0, mem::ocm0_addr>();
        arm_chain<mem::dma1, mem::ocm1_addr>();
        barrier();
        state = State::armed;
        return true;
    }

    bool trigger() {
        if (state != State::armed || !clock_locked()) return false;
        state = State::running;
        ctl.write<reg::trigger>(1);
        ctl.write<reg::trigger>(0);
        return true;
    }

    bool start() { return arm() && trigger(); }

    // 0: running/unarmed, 1: complete, 2/3: pair overflow, 4: DMA error,
    // 5: invalid capture settings, 6: ADC clock unlocked.
    uint32_t get_status() {
        if (state != State::running && state != State::complete) return 0;
        const auto status = sts.read<reg::capture_status>();
        if (status & capture_invalid) return 5;
        if (!clock_locked()) return 6;
        if (read_dmas<s2mm_dmasr>() & dma_errors) return 4;
        if (!(status & capture_done)) return 0;
        if (!(last_descriptor_status<mem::ocm0>() & descriptor_complete) ||
            !(last_descriptor_status<mem::ocm1>() & descriptor_complete)) return 0;
        barrier();
        // Wait for padded error records to drain before the client stops DMA.
        if (status & capture_overflow0) return 2;
        if (status & capture_overflow1) return 3;
        if (sts.read<reg::captured_samples>() != n_samples ||
            !check_descriptors<mem::ocm0>() || !check_descriptors<mem::ocm1>()) return 4;
        state = State::complete;
        return 1;
    }

    std::array<uint32_t, 6> get_diagnostics() {
        return {sts.read<reg::capture_status>(), sts.read<reg::captured_samples>(),
                hw::get_memory<mem::dma0>().read<s2mm_dmasr>(),
                hw::get_memory<mem::dma1>().read<s2mm_dmasr>(),
                last_descriptor_status<mem::ocm0>(), last_descriptor_status<mem::ocm1>()};
    }

    std::span<const uint32_t> get_adc_block(uint32_t pair, uint32_t index) {
        if (state != State::complete || pair > 1 || index >= n_desc) return {};
        const auto data = pair == 0 ? hw::get_memory<mem::ram0>().read_span<uint32_t>(n_samples)
                                    : hw::get_memory<mem::ram1>().read_span<uint32_t>(n_samples);
        return data.subspan(index * packet_samples, block_samples(index));
    }

    void stop() {
        ctl.write<reg::acq_reset>(1);
        ctl.write<reg::trigger>(0);
        write_dmas<s2mm_dmacr>(0);
        state = State::idle;
    }

  private:
    enum class State { idle, ready, armed, running, complete };

    static constexpr uint32_t sample_rate = 200000000;
    static constexpr uint32_t bytes_per_frame = sizeof(uint32_t); // Two 16-bit ADC codes.
    static constexpr uint32_t s2mm_dmacr = 0x30;
    static constexpr uint32_t s2mm_dmasr = 0x34;
    static constexpr uint32_t s2mm_curdesc = 0x38;
    static constexpr uint32_t s2mm_taildesc = 0x40;
    static constexpr uint32_t dma_run = 1;
    static constexpr uint32_t dma_reset = 4;
    static constexpr uint32_t dma_errors = 0x770;

    static constexpr uint32_t descriptor_stride = 64;
    static constexpr uint32_t descriptor_buffer = 8;
    static constexpr uint32_t descriptor_length = 24;
    static constexpr uint32_t descriptor_status = 28;
    static constexpr uint32_t descriptor_complete = 0x80000000U;
    static constexpr uint32_t descriptor_status_mask = 0xF0000000U;
    static constexpr uint32_t descriptor_length_mask = 0x7FFFFFU;

    // quad_capture status register bits (see quad_capture.v).
    static constexpr uint32_t capture_done = 1U << 1;
    static constexpr uint32_t capture_overflow0 = 1U << 2;
    static constexpr uint32_t capture_overflow1 = 1U << 3;
    static constexpr uint32_t capture_invalid = 1U << 4;

    static constexpr uint32_t max_samples = 16 * 1024 * 1024;
    static constexpr uint32_t packet_samples = 64 * 1024;
    static_assert(mem::ram0_range == max_samples * bytes_per_frame);
    static_assert(mem::ram1_range == max_samples * bytes_per_frame);
    static_assert(mem::ocm0_range >= (max_samples / packet_samples) * descriptor_stride);
    static_assert(mem::ocm1_range >= (max_samples / packet_samples) * descriptor_stride);
    uint32_t n_samples = 0;
    uint32_t n_desc = 0;
    State state = State::idle;
    hw::Memory<mem::control>& ctl = hw::get_memory<mem::control>();
    hw::Memory<mem::status>& sts = hw::get_memory<mem::status>();

    uint32_t block_samples(uint32_t index) const {
        return std::min(packet_samples, n_samples - index * packet_samples);
    }

    template<auto Descriptors, uint32_t RamAddress, uint32_t DescriptorAddress> void prepare() {
        auto& descriptors = hw::get_memory<Descriptors>();
        for (uint32_t i = 0; i < n_desc; ++i) {
            const uint32_t offset = descriptor_stride * i;
            for (uint32_t j = 0; j < descriptor_stride; j += sizeof(uint32_t)) {
                descriptors.write_reg(offset + j, 0U);
            }
            descriptors.write_reg(offset, DescriptorAddress + descriptor_stride * ((i + 1) % n_desc));
            descriptors.write_reg(offset + descriptor_buffer, RamAddress + packet_samples * bytes_per_frame * i);
            descriptors.write_reg(offset + descriptor_length, block_samples(i) * bytes_per_frame);
        }
    }

    template<auto Dma, uint32_t DescriptorAddress> void arm_chain() {
        auto& dma = hw::get_memory<Dma>();
        dma.template write<s2mm_curdesc>(DescriptorAddress);
        dma.template write<s2mm_dmacr>(dma_run); // Normal SG mode, finite tail, not cyclic.
        dma.template write<s2mm_taildesc>(DescriptorAddress + (n_desc - 1) * descriptor_stride);
    }

    template<auto Descriptors> bool check_descriptors() {
        auto& descriptors = hw::get_memory<Descriptors>();
        for (uint32_t i = 0; i < n_desc; ++i) {
            const auto descriptor = descriptors.read_reg(descriptor_stride * i + descriptor_status);
            if ((descriptor & descriptor_status_mask) != descriptor_complete ||
                (descriptor & descriptor_length_mask) != block_samples(i) * bytes_per_frame) return false;
        }
        return true;
    }

    bool clock_locked() {
        return (sts.read<reg::pll_locked>() & 1) != 0;
    }

    template<uint32_t Offset> uint32_t read_dmas() {
        return hw::get_memory<mem::dma0>().read<Offset>() |
               hw::get_memory<mem::dma1>().read<Offset>();
    }

    template<uint32_t Offset> void write_dmas(uint32_t value) {
        hw::get_memory<mem::dma0>().write<Offset>(value);
        hw::get_memory<mem::dma1>().write<Offset>(value);
    }

    bool reset_dmas() {
        write_dmas<s2mm_dmacr>(dma_reset);
        for (unsigned i = 0; i < 100; ++i) {
            if (!(read_dmas<s2mm_dmacr>() & dma_reset)) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return !(read_dmas<s2mm_dmacr>() & dma_reset);
    }

    template<auto Descriptors> uint32_t last_descriptor_status() {
        if (!n_desc) return 0;
        return hw::get_memory<Descriptors>().read_reg(
            descriptor_stride * (n_desc - 1) + descriptor_status);
    }

    void barrier() {
        (void)hw::get_memory<mem::dma0>().read<s2mm_dmasr>();
        (void)hw::get_memory<mem::dma1>().read<s2mm_dmasr>();
#if defined(__arm__)
        asm volatile("dsb sy" ::: "memory");
#else
        std::atomic_thread_fence(std::memory_order_seq_cst);
#endif
    }
};
#endif
