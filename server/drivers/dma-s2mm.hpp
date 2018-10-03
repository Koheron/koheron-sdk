/// DMA S2MM driver
///
/// (c) Koheron

// https://www.xilinx.com/support/documentation/ip_documentation/axi_dma/v7_1/pg021_axi_dma.pdf

#ifndef __SERVER_DRIVERS_DMA_S2MM_HPP__
#define __SERVER_DRIVERS_DMA_S2MM_HPP__

#include <context.hpp>

class DmaS2MM
{
  public:
    DmaS2MM(Context& ctx)
    : dma(ctx.mm.get<mem::dma>())
    , axi_hp0(ctx.mm.get<mem::axi_hp0>())
    {
        // Set AXI_HP0 to 32 bits
        axi_hp0.set_bit<0x0, 0>();
        axi_hp0.set_bit<0x14, 0>();
    }

    void start_transfer(uint32_t dest_addr, uint32_t length) {
        reset_dma();
        start_dma();
        set_destination_address(dest_addr);
        set_length(length);
    }

    // Ideally would take a std::chrono::duration as an argument
    void wait_for_transfer(float dma_transfer_duration_seconds) {
        auto dma_duration = std::chrono::milliseconds(uint32_t(1000 * dma_transfer_duration_seconds));

        while (! is_transfer_done()) {
            std::this_thread::sleep_for(0.25 * dma_duration);
        }
    }

  private:
    static constexpr uint32_t s2mm_dmacr = 0x30;  // S2MM DMA Control register
    static constexpr uint32_t s2mm_dmasr = 0x34;  // S2MM DMA Status register
    static constexpr uint32_t s2mm_da = 0x48;     // S2MM Destination Address
    static constexpr uint32_t s2mm_length = 0x58; // S2MM Buffer Length (Bytes)

    Memory<mem::dma>& dma;
    Memory<mem::axi_hp0>& axi_hp0;

    void reset_dma() {
        dma.set_bit<s2mm_dmacr, 2>();
    }

    void start_dma() {
        dma.set_bit<s2mm_dmacr, 0>();
    }

    void set_destination_address(uint32_t address) {
        dma.write<s2mm_da>(address);
    }

    void set_length(uint32_t length) {
        dma.write<s2mm_length>(length);
    }

    bool is_transfer_done() {
        return dma.read_bit<s2mm_dmasr, 1>();
    }
};

#endif // __SERVER_DRIVERS_DMA_S2MM_HPP__