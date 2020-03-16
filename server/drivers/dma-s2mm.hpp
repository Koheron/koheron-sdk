/// DMA S2MM driver
///
/// (c) Koheron

// https://www.xilinx.com/support/documentation/ip_documentation/axi_dma/v7_1/pg021_axi_dma.pdf

#ifndef __SERVER_DRIVERS_DMA_S2MM_HPP__
#define __SERVER_DRIVERS_DMA_S2MM_HPP__

#include <context.hpp>

#include <chrono>

class DmaS2MM
{
  public:
    DmaS2MM(Context& ctx_)
    : ctx(ctx_)
    , dma(ctx.mm.get<mem::dma>())
    , axi_hp0(ctx.mm.get<mem::axi_hp0>())
    {
        // Set AXI_HP0 to 32 bits
        axi_hp0.set_bit<0x0, 0>();
        axi_hp0.set_bit<0x14, 0>();
    }

    void start_transfer(uint32_t dest_addr, uint32_t length) {
        reset();
        start();
        set_destination_address(dest_addr);
        set_length(length);
    }

    // Ideally would take a std::chrono::duration as an argument
    void wait_for_transfer(float dma_transfer_duration_seconds) {
        const auto dma_duration = std::chrono::milliseconds(uint32_t(1000 * dma_transfer_duration_seconds));
        uint32_t cnt = 0;

        while (! idle()) {
            std::this_thread::sleep_for(0.55 * dma_duration);
            cnt++;

            if (cnt > max_sleeps_cnt) {
                ctx.log<ERROR>("DmaS2MM::wait_for_transfer: Max number of sleeps exceeded. [set duration %f s]\n",
                               double(dma_transfer_duration_seconds));
                break;
            }
        }
    }

  private:
    static constexpr uint32_t s2mm_dmacr  = 0x30;  // S2MM DMA Control register
    static constexpr uint32_t s2mm_dmasr  = 0x34;  // S2MM DMA Status register
    static constexpr uint32_t s2mm_da     = 0x48;  // S2MM Destination Address
    static constexpr uint32_t s2mm_length = 0x58;  // S2MM Buffer Length (Bytes)

    static constexpr uint32_t max_sleeps_cnt = 4;

    Context& ctx;
    Memory<mem::dma>& dma;
    Memory<mem::axi_hp0>& axi_hp0;

    void reset() {
        dma.set_bit<s2mm_dmacr, 2>();

        // Wait for reset
        uint32_t cnt = 0;

        while (dma.read_bit<s2mm_dmacr, 2>()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            cnt++;

            if (cnt > max_sleeps_cnt) {
                ctx.log<ERROR>("DmaS2MM::reset: Max number of sleeps exceeded.\n");
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
                ctx.log<ERROR>("DmaS2MM::start: Max number of sleeps exceeded.\n");
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