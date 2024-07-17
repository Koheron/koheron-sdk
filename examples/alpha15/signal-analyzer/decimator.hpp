/// Pulse driver
///
/// (c) Koheron

#ifndef __DRIVERS_SIGNAL_ANALYZER_HPP__
#define __DRIVERS_SIGNAL_ANALYZER_HPP__

#include <context.hpp>

#include <boards/alpha15/drivers/clock-generator.hpp>

// http://www.xilinx.com/support/documentation/ip_documentation/axi_fifo_mm_s/v4_1/pg080-axi-fifo-mm-s.pdf
namespace Fifo_regs {
    constexpr uint32_t rdfr = 0x18;
    constexpr uint32_t rdfo = 0x1C;
    constexpr uint32_t rdfd = 0x20;
    constexpr uint32_t rlr = 0x24;
}

class Decimator
{
  public:
    Decimator(Context& ctx_)
    : ctx(ctx_)
    , ctl(ctx.mm.get<mem::control>())
    , sts(ctx.mm.get<mem::status>())
    , adc_fifo_map(ctx.mm.get<mem::adc_fifo>())
    , clk_gen(ctx.get<ClockGenerator>())
    {
        fs_adc = clk_gen.get_adc_sampling_freq()[0];
        set_cic_rate(prm::cic_decimation_rate_default);
        fifo_transfer_duration = n_pts / fs;
        ctx.log<INFO>("Decimator: FIFO transfer duration = %f s\n", double(fifo_transfer_duration));
    }

    void set_cic_rate(uint32_t rate) {
        if (rate < prm::cic_decimation_rate_min ||
            rate > prm::cic_decimation_rate_max) {
            ctx.log<ERROR>("Decimator: CIC rate out of range\n");
            return;
        }

        cic_rate = rate;
        fs = fs_adc / (2.0f * cic_rate); // Sampling frequency (factor of 2 because of FIR)
        fifo_transfer_duration = n_pts / fs;
        ctl.write<reg::cic_rate>(cic_rate);
    }

    // Adc FIFO => TODO FIFO driver

    uint32_t get_fifo_occupancy() {
        return adc_fifo_map.read<Fifo_regs::rdfo>();
    }

    void reset_fifo() {
        adc_fifo_map.write<Fifo_regs::rdfr>(0x000000A5);
    }

    uint32_t read_fifo() {
        return adc_fifo_map.read<Fifo_regs::rdfd>();
    }

    uint32_t get_fifo_length() {
        return (adc_fifo_map.read<Fifo_regs::rlr>() & 0x3FFFFF) >> 2;
    }

    void wait_for_transfer(float fifo_transfer_duration_seconds) {
        const auto fifo_duration = std::chrono::milliseconds(uint32_t(1000 * fifo_transfer_duration_seconds));
        uint32_t cnt = 0;

        while (get_fifo_length() < n_pts) {
            std::this_thread::sleep_for(0.55 * fifo_duration);
            cnt++;

            if (cnt > max_sleeps_cnt) {
                ctx.log<ERROR>("Fifo::wait_for_transfer: Max number of sleeps exceeded. [set duration %f s]\n",
                               double(fifo_transfer_duration_seconds));
                break;
            }
        }
    }

    auto read_adc() {
        wait_for_transfer(fifo_transfer_duration);

        for (unsigned int i=0; i < n_pts; i++) {
            adc_data[i] = read_fifo();
        }

        return adc_data;
    }

  private:
    static constexpr uint32_t n_pts = 8192;
    static constexpr uint32_t max_sleeps_cnt = 4;

    Context& ctx;
    Memory<mem::control>& ctl;
    Memory<mem::status>& sts;
    Memory<mem::adc_fifo>& adc_fifo_map;

    ClockGenerator& clk_gen;

    uint32_t cic_rate;
    float fs_adc, fs;
    float fifo_transfer_duration;

    std::array<uint32_t, n_pts> adc_data;
};

#endif // __DRIVERS_SIGNAL_ANALYZER_HPP__
