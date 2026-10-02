#ifndef ADC_DMA_CHIRP_HPP
#define ADC_DMA_CHIRP_HPP

#include "server/hardware/memory_manager.hpp"
#include <chrono>
#include <cstdint>
#include <span>
#include <thread>
#include <vector>

// One 128 MiB capture, using 512 descriptors of 256 KiB in 32 KiB of OCM.
class AdcDma {
  public:
    AdcDma() {
        sclr.write<0x8>(0xDF0D);       // Unlock SLCR.
        sclr.write<0x910>(0b1000);    // Last 64 KiB of OCM at 0xFFFF0000.
        hp.clear_bit<0x0, 0>();       // 64-bit HP0 read/write interfaces.
        hp.clear_bit<0x14, 0>();
        ctl.write<reg::acq_reset>(1);
    }

    // Return false on invalid settings or a DMA reset timeout.
    bool configure(uint32_t samples, uint64_t coefficient,
                   const std::vector<uint64_t>& seeds) {
        ready = false;
        completed = false;
        ctl.write<reg::acq_reset>(1);
        ctl.write<reg::trigger>(0);
        ctl.write<reg::seed_write>(0);
        dma.write<0x30>(4);           // S2MM soft reset.
        for (unsigned i = 0; i < 100 && (dma.read<0x30>() & 4); ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if ((dma.read<0x30>() & 4) || seeds.size() != 16 ||
            samples <= 2 * (1U << 18) || samples > n_samples - 1024 ||
            coefficient == 0 || coefficient >= (1ULL << 48)) return false;
        for (auto seed : seeds)
            if (seed == 0 || seed >= (1ULL << 63)) return false;

        ctl.write<reg::chirp_samples>(samples);
        ctl.write<reg::chirp_coefficient0>(uint32_t(coefficient));
        ctl.write<reg::chirp_coefficient1>(uint32_t(coefficient >> 32));
        // Hold stream reset long enough to flush both sides of the CDC FIFO.
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        ctl.write<reg::acq_reset>(0);
        for (uint32_t i = 0; i < 16; ++i) {
            ctl.write<reg::seed_index>(i);
            ctl.write<reg::seed_data0>(uint32_t(seeds[i]));
            ctl.write<reg::seed_data1>(uint32_t(seeds[i] >> 32));
            ctl.write<reg::seed_write>(1);
            ctl.write<reg::seed_write>(0);
        }
        for (uint32_t i = 0; i < n_desc; ++i) {
            const uint32_t offset = 64 * i;
            for (uint32_t j = 0; j < 64; j += 4) ocm.write_reg(offset + j, 0U);
            ocm.write_reg(offset, mem::ocm_s2mm_addr + 64 * ((i + 1) % n_desc));
            ocm.write_reg(offset + 8, mem::ram_s2mm_addr + packet_bytes * i);
            ocm.write_reg(offset + 24, packet_bytes);
        }
        barrier();
        ready = true;
        return true;
    }

    bool start() {
        if (!ready) return false;
        ready = false; // Reconfigure/reseed before another acquisition.
        dma.write<0x38>(mem::ocm_s2mm_addr);
        dma.write<0x30>(1);
        dma.write<0x40>(mem::ocm_s2mm_addr + (n_desc - 1) * 64);
        barrier();
        ctl.write<reg::trigger>(1);
        ctl.write<reg::trigger>(0);
        return true;
    }

    // 0: running, 1: complete, 2: overflow, 3: clipping, 4: DMA error.
    uint32_t get_status() {
        const auto status = sts.read<reg::capture_status>();
        if (status & 4) return 2;
        if (dma.read<0x34>() & 0x770) return 4;
        if (!(status & 2)) return 0;
        if (!(ocm.read_reg((n_desc - 1) * 64 + 28) & 0x80000000U)) return 0;
        barrier();
        for (uint32_t i = 0; i < n_desc; ++i) {
            const auto descriptor = ocm.read_reg(64 * i + 28);
            if ((descriptor & 0xF0000000U) != 0x80000000U ||
                (descriptor & 0x7FFFFFU) != packet_bytes) return 4;
        }
        if (status & 8) return 3;
        completed = true;
        return 1;
    }

    // Chunked transfer avoids allocating another 128 MiB on the board.
    std::span<const uint32_t> get_adc_block(uint32_t index) {
        if (!completed || index >= n_desc) return {};
        const auto data = ram.read_span<uint32_t>(n_samples / 2);
        return data.subspan(index * packet_bytes / 4, packet_bytes / 4);
    }

    void stop() {
        ctl.write<reg::acq_reset>(1);
        dma.write<0x30>(0);
        ready = false;
        completed = false;
    }

  private:
    static constexpr uint32_t n_samples = 64 * 1024 * 1024;
    static constexpr uint32_t n_desc = 512;
    static constexpr uint32_t packet_bytes = 256 * 1024;
    bool ready = false;
    bool completed = false;
    hw::Memory<mem::control>& ctl = hw::get_memory<mem::control>();
    hw::Memory<mem::status>& sts = hw::get_memory<mem::status>();
    hw::Memory<mem::dma>& dma = hw::get_memory<mem::dma>();
    hw::Memory<mem::ram_s2mm>& ram = hw::get_memory<mem::ram_s2mm>();
    hw::Memory<mem::ocm_s2mm>& ocm = hw::get_memory<mem::ocm_s2mm>();
    hw::Memory<mem::axi_hp0>& hp = hw::get_memory<mem::axi_hp0>();
    hw::Memory<mem::sclr>& sclr = hw::get_memory<mem::sclr>();

    void barrier() {
        (void)dma.read<0x34>();
        asm volatile("dsb sy" ::: "memory");
    }
};
#endif
