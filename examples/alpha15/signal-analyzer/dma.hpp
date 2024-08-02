/// DMA driver
///
/// (c) Koheron

#ifndef __DRIVERS_DMA_HPP__
#define __DRIVERS_DMA_HPP__

#include <context.hpp>

// https://www.xilinx.com/support/documentation/ip_documentation/axi_dma/v7_1/pg021_axi_dma.pdf
namespace Dma_regs {
    constexpr uint32_t s2mm_dmacr = 0x30;  // S2MM DMA Control register
    constexpr uint32_t s2mm_dmasr = 0x34;  // S2MM DMA Status register
    constexpr uint32_t s2mm_da = 0x48;     // S2MM Destination Address
    constexpr uint32_t s2mm_length = 0x58; // S2MM Buffer Length (Bytes)
}

class Dma
{
  public:
    Dma(Context& ctx)
    : dma(ctx.mm.get<mem::dma>())
    , ram(ctx.mm.get<mem::ram>())
    , axi_hp0(ctx.mm.get<mem::axi_hp0>())
    {
        // Set AXI_HP0 to 32 bits
        axi_hp0.set_bit<0x0, 0>();
        axi_hp0.set_bit<0x14, 0>();
    }

    void reset_dma() {
        dma.set_bit<Dma_regs::s2mm_dmacr, 2>();
    }

    void start_dma() {
        dma.set_bit<Dma_regs::s2mm_dmacr, 0>();
    }

    void set_destination_address(uint32_t address) {
        dma.write<Dma_regs::s2mm_da>(address);
    }

    void set_length(uint32_t length) {
        dma.write<Dma_regs::s2mm_length>(length);
    }

    auto& get_data() {
        reset_dma();
        start_dma();
        set_destination_address(mem::ram_addr);
        set_length(4 * n_pts);
        data = ram.read_array<int32_t, 1000000, 12288>();
        return data;
    }

  private:
    Memory<mem::dma>& dma;
    Memory<mem::ram>& ram;
    Memory<mem::axi_hp0>& axi_hp0;
    static constexpr uint32_t n_pts = 1024*1024;
    std::array<int32_t, 1000000> data;

} ;

#endif // __DRIVERS_DMA_HPP__
