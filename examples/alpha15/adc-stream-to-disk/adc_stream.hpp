/// Continuous ALPHA15 ADC0 capture into a DDR descriptor ring.
#ifndef __ALPHA15_ADC_STREAM_HPP__
#define __ALPHA15_ADC_STREAM_HPP__

#include "server/hardware/memory_manager.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <thread>

class AdcStream {
  public:
    static constexpr uint32_t words_per_chunk = 128 * 1024;
    static constexpr uint32_t chunk_bytes = 4 * words_per_chunk;
    static constexpr uint32_t descriptor_count = 32;

    AdcStream()
    : ctl(hw::get_memory<mem::control>())
    , dma(hw::get_memory<mem::dma>())
    , ram(hw::get_memory<mem::ram_s2mm>())
    , descriptors(hw::get_memory<mem::ocm_s2mm>())
    , adc_status(hw::get_memory<mem::status>())
    , hp(hw::get_memory<mem::axi_hp2>())
    , sclr(hw::get_memory<mem::sclr>())
    {
        sclr.write<0x8>(0xDF0D);
        sclr.write_mask<0x910, 0x8>(0x8); // Map the last 64 KiB of OCM high.
        sclr.clear_bit<0x240, 1>();
        hp.clear_bit<0x0, 0>();
        hp.clear_bit<0x14, 0>();
    }

    void set_test_tone(uint32_t enable) {
        ctl.write<reg::test_tone_enable>(enable ? 1U : 0U);
    }

    uint32_t start() {
        stop();
        error_ = 0;
        dma.set_bit<0x30, 2>(); // S2MM reset.
        for (unsigned i = 0; dma.read_bit<0x30, 2>(); ++i) {
            if (i == 1000) return error_ = 1;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        for (uint32_t i = 0; i < descriptor_count; ++i) {
            const uint32_t offset = 0x40 * i;
            descriptors.write_reg(offset + 0x00, mem::ocm_s2mm_addr + 0x40 * ((i + 1) % descriptor_count));
            descriptors.write_reg(offset + 0x08, mem::ram_s2mm_addr + i * chunk_bytes);
            descriptors.write_reg(offset + 0x18, chunk_bytes);
            descriptors.write_reg(offset + 0x1C, 0);
        }

        next_ = 0;
        discard_first_ = true;
        dma.write<0x38>(mem::ocm_s2mm_addr);
        dma.write<0x30>(1); // Start S2MM, with cyclic mode disabled.
        dma.write<0x40>(mem::ocm_s2mm_addr + 0x40 * (descriptor_count - 1));
        overflow_start_ = adc_status.read<reg::adc_overflow>();
        running_ = true;
        return 0;
    }

    void stop() {
        if (running_) {
            dma.clear_bit<0x30, 0>();
            running_ = false;
        }
    }

    uint32_t get_error() {
        if (running_ && adc_status.read<reg::adc_overflow>() != overflow_start_) fail(7);
        if (running_ && dma.read_bit<0x34, 1>()) fail(5);
        return error_;
    }

    // Send four descriptors in one RPC to reduce command overhead.
    auto& read_batch() {
        for (uint32_t i = 0; i < 4; ++i) {
            if (!read_one(batch_.data() + i * words_per_chunk)) break;
        }
        return batch_;
    }

  private:
    bool read_one(uint32_t* destination) {
        if (!running_) {
            if (error_ == 0) error_ = 2;
            return false;
        }

        for (;;) {
            const uint32_t offset = 0x40 * next_;
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
            uint32_t status = 0;
            do {
                status = descriptors.read_reg(offset + 0x1C);
                const uint32_t dma_status = dma.read<0x34>();
                if (dma_status & 0x770) {
                    fail(3);
                    return false;
                }
                if (std::chrono::steady_clock::now() >= deadline) {
                    fail(4);
                    return false;
                }
                if (!(status & 0x80000000)) {
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                }
            } while (!(status & 0x80000000));
            // An idle S2MM channel means the producer caught the tail.
            if (dma.read_bit<0x34, 1>()) {
                fail(5);
                return false;
            }
            // The first packet can be short while the DMA receiver starts.
            // Discard it to start the file on a complete packet boundary.
            if (!discard_first_ && (status & 0x3FFFFFF) != chunk_bytes) {
                fail(6);
                return false;
            }
            if (!discard_first_) {
                std::memcpy(destination, ram.get_reg_ptr<uint32_t>(next_ * chunk_bytes), chunk_bytes);
            }

            descriptors.write_reg(offset + 0x1C, 0);
            dma.write<0x40>(mem::ocm_s2mm_addr + offset);
            next_ = (next_ + 1) % descriptor_count;
            if (discard_first_) {
                discard_first_ = false;
                continue;
            }
            return true;
        }
    }

    void fail(uint32_t code) {
        error_ = code;
        stop();
    }

    hw::Memory<mem::control>& ctl;
    hw::Memory<mem::dma>& dma;
    hw::Memory<mem::ram_s2mm>& ram;
    hw::Memory<mem::ocm_s2mm>& descriptors;
    hw::Memory<mem::status>& adc_status;
    hw::Memory<mem::axi_hp2>& hp;
    hw::Memory<mem::sclr>& sclr;
    std::array<uint32_t, 4 * words_per_chunk> batch_{};
    uint32_t next_ = 0;
    bool running_ = false;
    bool discard_first_ = true;
    uint32_t error_ = 0;
    uint32_t overflow_start_ = 0;
};

#endif
