/// DMA driver
///
/// (c) Koheron

#ifndef __DRIVERS_DMA_HPP__
#define __DRIVERS_DMA_HPP__

#include <context.hpp>

#include <boards/alpha250/drivers/clock-generator.hpp>
#include <server/drivers/dma-s2mm.hpp>

class Dma
{
  public:
    Dma(Context& ctx_)
    : ctx(ctx_)
    , dma(ctx.get<DmaS2MM>())
    , clk_gen(ctx.get<ClockGenerator>())
    , ctl(ctx.mm.get<mem::control>())
    , sts(ctx.mm.get<mem::status>())
    , ram(ctx.mm.get<mem::ram>())
    {
        fs_adc = clk_gen.get_adc_sampling_freq();
        set_cic_rate(prm::cic_decimation_rate_default);
        ctx.log<INFO>("DMA transfer duration = %f s\n", double(dma_transfer_duration));
    }

    void set_cic_rate(uint32_t rate) {
        if (rate < prm::cic_decimation_rate_min ||
            rate > prm::cic_decimation_rate_max) {
            ctx.log<ERROR>("DMA: CIC rate out of range\n");
            return;
        }

        cic_rate = rate;
        fs = fs_adc / (2.0f * cic_rate); // Sampling frequency (factor of 2 because of FIR)
        dma_transfer_duration = prm::n_pts / fs;
        ctl.write<reg::cic_rate>(cic_rate);
    }

    auto& get_data() {
        dma.start_transfer(mem::ram_addr, sizeof(int32_t) * prm::n_pts);
        dma.wait_for_transfer(dma_transfer_duration);
        data = ram.read_array<int32_t, data_size, read_offset>();
        return data;
    }

    uint32_t get_data_size() {
        return data_size;
    }

    uint32_t get_sampling_frequency() {
        return fs;
    }

  private:
    static constexpr uint32_t data_size = 1000000;
    static constexpr uint32_t read_offset = (prm::n_pts - data_size) / 2;

    Context& ctx;
    DmaS2MM& dma;
    ClockGenerator& clk_gen;
    Memory<mem::control>& ctl;
    Memory<mem::status>& sts;
    Memory<mem::ram>& ram;

    uint32_t cic_rate;
    float fs_adc, fs;
    float dma_transfer_duration;

    std::array<int32_t, data_size> data;

} ;

#endif // __DRIVERS_DMA_HPP__
