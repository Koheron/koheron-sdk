/// DMA S2MM driver
///
/// (c) Koheron

// https://www.xilinx.com/support/documentation/ip_documentation/axi_dma/v7_1/pg021_axi_dma.pdf

#ifndef __SERVER_DRIVERS_DMA_S2MM_HPP__
#define __SERVER_DRIVERS_DMA_S2MM_HPP__

#include "server/runtime/syslog.hpp"
#include "server/hardware/memory_manager.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <thread>
#include <scicpp/core.hpp>

class DmaS2MM
{
  public:
    DmaS2MM()
    : dma(hw::get_memory<mem::dma>())
    , axi_hp0(hw::get_memory<mem::axi_hp0>())
    {
        // Set AXI_HP0 to 32 bits
        axi_hp0.set_bit<0x0, 0>();
        axi_hp0.set_bit<0x14, 0>();
    }

    void start_transfer(uint32_t dest_addr, uint32_t length) {
        // A cyclic SG consumer owns the descriptor ring. Legacy simple-mode
        // RPCs must never reset it or write a raw destination into that engine.
        if (dma.read<s2mm_dmasr>() & 8u) {
            log<ERROR>("Simple DMA transfer is unavailable on an SG engine\n");
            return;
        }
        reset();
        start();
        set_destination_address(dest_addr);
        set_length(length);
        transfer_started = std::chrono::steady_clock::now();
    }

    template<MemID id, std::size_t n_elems, class T>
    void start_transfer() {
        using memory = hw::Memory<id>;
        constexpr auto transfer_size = n_elems * sizeof(T);

        static_assert(n_elems > 0);
        static_assert(std::is_trivially_copyable_v<T>); // Trivial types avoid surprises in sizeof(T)
        static_assert(transfer_size <= memory::size);

        start_transfer(memory::phys_addr, transfer_size);
    }

    // Preserve the existing RPC and C++ interface for other instruments.
    template<typename T>
    void wait_for_transfer(scicpp::units::time<T> duration) {
        wait_for_transfer(duration.eval());
    }

    void wait_for_transfer(float duration_seconds) {
        (void)wait_for_transfer_checked(duration_seconds);
    }

    template<typename T>
    bool wait_for_transfer_checked(scicpp::units::time<T> dma_transfer_duration) {
        return wait_for_transfer_checked(dma_transfer_duration.eval());
    }

    bool wait_for_transfer_checked(float dma_transfer_duration_seconds) {
        if (dma.read<s2mm_dmasr>() & 8u) return false;
        if (idle()) return (dma.read<s2mm_dmasr>() & 0x70u) == 0;
        if (!std::isfinite(dma_transfer_duration_seconds) || dma_transfer_duration_seconds <= 0.0f)
            return false;
        using Clock = std::chrono::steady_clock;
        const auto duration = std::chrono::duration_cast<Clock::duration>(
            std::chrono::duration<float>(dma_transfer_duration_seconds));
        const auto margin = std::min(duration / 20,
            std::chrono::duration_cast<Clock::duration>(std::chrono::milliseconds(1)));
        const auto poll_start = transfer_started + duration - margin;
        const auto deadline = transfer_started + std::max(3 * duration,
            std::chrono::duration_cast<Clock::duration>(std::chrono::milliseconds(5)));

        while (! idle()) {
            if (dma.read<s2mm_dmasr>() & 0x70u) {
                log<ERROR>("DmaS2MM::wait_for_transfer: DMA transfer error\n");
                return false;
            }
            const auto now = Clock::now();
            if (now >= deadline) {
                logf<ERROR>(
                    "DmaS2MM::wait_for_transfer: Deadline exceeded. [set duration {} s]\n",
                    dma_transfer_duration_seconds);
                return false;
            }
            // Account for processing overlapped with DMA. Sleep near completion,
            // then poll briefly instead of rounding up by half a whole capture.
            std::this_thread::sleep_until(std::min(deadline,
                std::max(poll_start, now + std::chrono::microseconds(200))));
        }
        return (dma.read<s2mm_dmasr>() & 0x70u) == 0;
    }

  private:
    std::chrono::steady_clock::time_point transfer_started{};
    static constexpr uint32_t s2mm_dmacr  = 0x30;  // S2MM DMA Control register
    static constexpr uint32_t s2mm_dmasr  = 0x34;  // S2MM DMA Status register
    static constexpr uint32_t s2mm_da     = 0x48;  // S2MM Destination Address
    static constexpr uint32_t s2mm_length = 0x58;  // S2MM Buffer Length (Bytes)

    static constexpr uint32_t max_sleeps_cnt = 4;

    hw::Memory<mem::dma>& dma;
    hw::Memory<mem::axi_hp0>& axi_hp0;

    void reset() {
        dma.set_bit<s2mm_dmacr, 2>();

        // Wait for reset
        uint32_t cnt = 0;

        while (dma.read_bit<s2mm_dmacr, 2>()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            cnt++;

            if (cnt > max_sleeps_cnt) {
                log<ERROR>("DmaS2MM::reset: Max number of sleeps exceeded.\n");
                break;
            }
        }
    }

    void start() {
        dma.set_bit<s2mm_dmacr, 0>();

        // Wait for start up
        uint32_t cnt = 0;

        while (halted()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            cnt++;

            if (cnt > max_sleeps_cnt) {
                log<ERROR>("DmaS2MM::start: Max number of sleeps exceeded.\n");
                break;
            }
        }
    }

    void set_destination_address(uint32_t address) {
        dma.write<s2mm_da>(address);
    }

    void set_length(uint32_t length) {
        dma.write<s2mm_length>(length);
    }

    // Status

    bool halted() {
        return dma.read_bit<s2mm_dmasr, 0>();
    }

    bool idle() {
        return dma.read_bit<s2mm_dmasr, 1>();
    }
};

#endif // __SERVER_DRIVERS_DMA_S2MM_HPP__
