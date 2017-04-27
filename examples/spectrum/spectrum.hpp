/// Spectrum analyzer driver
///
/// (c) Koheron

#ifndef __DRIVERS_SPECTRUM_HPP__
#define __DRIVERS_SPECTRUM_HPP__

#include <context.hpp>
#include <cmath>

constexpr float SAMPLING_RATE = 125E6;
constexpr uint32_t WFM_SIZE = mem::spectrum_range/sizeof(float);
constexpr uint32_t FIFO_BUFF_SIZE = 4096;

// http://www.xilinx.com/support/documentation/ip_documentation/axi_fifo_mm_s/v4_1/pg080-axi-fifo-mm-s.pdf
namespace Fifo_regs {
    constexpr uint32_t rdfr = 0x18;
    constexpr uint32_t rdfo = 0x1C;
    constexpr uint32_t rdfd = 0x20;
    constexpr uint32_t rlr = 0x24;
}

class Spectrum
{
  public:
    Spectrum(Context& ctx_)
    : ctx(ctx_)
    , ctl(ctx.mm.get<mem::control>())
    , sts(ctx.mm.get<mem::status>())
    , spectrum_map(ctx.mm.get<mem::spectrum>())
    , demod_map(ctx.mm.get<mem::demod>())
    , peak_fifo_map(ctx.mm.get<mem::peak_fifo>())
    , noise_floor_map(ctx.mm.get<mem::noise_floor>())
    , data_decim(0)
    {
        raw_data = spectrum_map.get_ptr<float>();
        set_average(true);
        ctl.write<reg::addr>(19 << 2); // set tvalid delay to 19 * 8 ns
        set_address_range(0, WFM_SIZE);
        set_period(WFM_SIZE);
        set_num_average_min(0);

        std::array<uint32_t,WFM_SIZE> demod_buffer;
        demod_buffer.fill(0x00003FFF);
        set_scale_sch(0);
        set_demod_buffer(demod_buffer);
    }

    // Averaging

    auto get_average_status() {
        return std::make_tuple(
            is_average,
            num_average_min,
            num_average
        );
    }

    void set_average(bool is_average_) {
        is_average = is_average_;
        ctl.write_bit<reg::avg, 0>(is_average);
    }

    uint32_t get_num_average()  {
        num_average = sts.read<reg::n_avg>();
        return num_average;
    }

    void set_num_average_min(uint32_t num_average_min_) {
        num_average_min = (num_average_min_ < 2) ? 0 : num_average_min_ - 2;
        ctl.write<reg::n_avg_min>(num_average_min);
    }

    // Acquisition

    void reset_acquisition() {
        ctl.clear_bit<reg::addr, 1>();
        ctl.set_bit<reg::addr, 1>();
    }

    void set_scale_sch(uint32_t scale_sch) {
        // LSB at 1 for forward FFT
        ctl.write<reg::ctl_fft>(1 + 2 * scale_sch);
    }

    void set_offset(uint32_t offset_real, uint32_t offset_imag) {
        ctl.write<reg::substract_mean>(offset_real + 16384 * offset_imag);
    }

    void set_demod_buffer(const std::array<uint32_t, WFM_SIZE>& arr) {
        demod_map.write_array(arr);
    }

    void set_noise_floor_buffer(const std::array<float, WFM_SIZE>& arr) {
        noise_floor_map.write_array(arr);
    }

    // Read channel and take one point every decim_factor points
    std::vector<float>& get_data_decim(uint32_t decim_factor, uint32_t index_low, uint32_t index_high) {
        // Sanity checks
        if (index_high <= index_low || index_high >= WFM_SIZE) {
            data_decim.resize(0);
            return data_decim;
        }

        ctl.set_bit<reg::addr, 1>();
        uint32_t n_pts = (index_high - index_low)/decim_factor;
        data_decim.resize(n_pts);
        wait_for_acquisition();

        if (sts.read<reg::avg_on_out>()) {
            float num_average_ = float(get_num_average());

            for (unsigned int i=0; i<data_decim.size(); i++)
                data_decim[i] = raw_data[index_low + decim_factor * i] / num_average_;
        } else {
            for (unsigned int i=0; i<data_decim.size(); i++)
                data_decim[i] = raw_data[index_low + decim_factor * i];
        }

        ctl.clear_bit<reg::addr, 1>();
        return data_decim;
    }

    // Peak

    uint32_t get_peak_address() {return sts.read<reg::peak_address>();}
    uint32_t get_peak_maximum() {return sts.read<reg::peak_maximum>();}

    void set_address_range(uint32_t address_low, uint32_t address_high) {
        ctl.write<reg::peak_address_low>(address_low);
        ctl.write<reg::peak_address_high>(address_high);
        ctl.write<reg::peak_address_reset>((address_low + WFM_SIZE - 1) % WFM_SIZE);
    }

    uint32_t read_fifo() {
        return peak_fifo_map.read<Fifo_regs::rdfd>();
    }

    uint32_t get_fifo_length() {
        return (peak_fifo_map.read<Fifo_regs::rlr>() & 0x3FFFFF) >> 2;
    }

    std::vector<uint32_t>& get_peak_fifo_data() {
        uint32_t n_pts = get_fifo_length();
        peak_fifo_data.resize(n_pts);

        for (unsigned int i=0; i < n_pts; i++) {
            peak_fifo_data[i] = read_fifo();
        }
        return peak_fifo_data;
    }

  private:

    bool is_average;
    uint32_t num_average_min;
    uint32_t num_average;

    Context& ctx;
    Memory<mem::control>& ctl;
    Memory<mem::status>& sts;
    Memory<mem::spectrum>& spectrum_map;
    Memory<mem::demod>& demod_map;
    Memory<mem::peak_fifo>& peak_fifo_map;
    Memory<mem::noise_floor>& noise_floor_map;

    // Acquired data buffers
    float *raw_data;
    std::array<float, WFM_SIZE> spectrum_data;
    std::vector<float> data_decim;
    std::vector<uint32_t> peak_fifo_data;

    // Internal functions
    void wait_for_acquisition() {
       do {} while (sts.read<reg::avg_ready>() == 0);
    }

    void set_period(uint32_t period) {
        set_dac_period(period, period);
        set_average_period(period);
        reset();
    }

    void reset() {
        ctl.clear_bit<reg::addr, 0>();
        ctl.set_bit<reg::addr, 0>();
    }

    void set_dac_period(uint32_t dac_period0, uint32_t dac_period1) {
        ctl.write<reg::dac_period0>(dac_period0 - 1);
        ctl.write<reg::dac_period1>(dac_period1 - 1);
    }

    void set_average_period(uint32_t average_period) {
        ctl.write<reg::avg_period>(average_period - 1);
        ctl.write<reg::avg_threshold>(average_period - 6);
    }

};

#endif // __DRIVERS_SPECTRUM_HPP__
