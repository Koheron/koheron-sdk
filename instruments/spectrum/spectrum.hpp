/// Spectrum analyzer driver
///
/// (c) Koheron

#ifndef __DRIVERS_SPECTRUM_HPP__
#define __DRIVERS_SPECTRUM_HPP__

#include <context.hpp>
#include <drivers/dac_router.hpp>

#define SAMPLING_RATE 125E6
#define WFM_SIZE mem::spectrum_range/sizeof(float)
#define FIFO_BUFF_SIZE 4096

// http://www.xilinx.com/support/documentation/ip_documentation/axi_fifo_mm_s/v4_1/pg080-axi-fifo-mm-s.pdf
#define FIFO_RDFR_OFF 0x18
#define FIFO_RDFO_OFF 0x1C
#define FIFO_RDFD_OFF 0x20
#define FIFO_RLR_OFF 0x24

class Spectrum
{
  public:
    Spectrum(Context& ctx)
    : cfg(ctx.mm.get<mem::config>())
    , sts(ctx.mm.get<mem::status>())
    , spectrum_map(ctx.mm.get<mem::spectrum>())
    , demod_map(ctx.mm.get<mem::demod>())
    , peak_fifo_map(ctx.mm.get<mem::peak_fifo>())
    , noise_floor_map(ctx.mm.get<mem::noise_floor>())
    , spectrum_decim(0)
    , dac(ctx)
    {
        raw_data = spectrum_map.get_ptr<float>();
        set_averaging(true);
        cfg.write<reg::addr>(19 << 2); // set tvalid delay to 19 * 8 ns
        set_address_range(0, WFM_SIZE);
        set_period(WFM_SIZE);
        set_n_avg_min(0);
        dac.set_config_reg(reg::dac_select, reg::addr_select);
    }

    void reset() {
        cfg.clear_bit<reg::addr, 0>();
        cfg.set_bit<reg::addr, 0>();
    }

    void set_n_avg_min(uint32_t n_avg_min) {
        uint32_t n_avg_min_ = (n_avg_min < 2) ? 0 : n_avg_min-2;
        cfg.write<reg::n_avg_min>(n_avg_min_);
    }

    void set_period(uint32_t period) {
        set_dac_period(period, period);
        set_avg_period(period);
        reset();
    }

    void set_dac_period(uint32_t dac_period0, uint32_t dac_period1) {
        cfg.write<reg::dac_period0>(dac_period0 - 1);
        cfg.write<reg::dac_period1>(dac_period1 - 1);
    }

    void set_avg_period(uint32_t avg_period) {
        cfg.write<reg::avg_period>(avg_period - 1);
        cfg.write<reg::avg_threshold>(avg_period - 6);
    }

    void set_dac_buffer(uint32_t channel, const std::array<uint32_t, WFM_SIZE/2>& arr) {
        dac.set_data(channel, arr);
    }

    std::array<uint32_t, WFM_SIZE/2>& get_dac_buffer(uint32_t channel) {
        return dac.get_data<WFM_SIZE/2>(channel);
    }

    void reset_acquisition() {
        cfg.clear_bit<reg::addr, 1>();
        cfg.set_bit<reg::addr, 1>();
    }

    void set_scale_sch(uint32_t scale_sch) {
        // LSB at 1 for forward FFT
        cfg.write<reg::cfg_fft>(1 + 2 * scale_sch);
    }

    void set_offset(uint32_t offset_real, uint32_t offset_imag) {
        cfg.write<reg::substract_mean>(offset_real + 16384 * offset_imag);
    }

    void set_demod_buffer(const std::array<uint32_t, WFM_SIZE>& arr) {
        demod_map.write_array(arr);
    }

    void set_noise_floor_buffer(const std::array<float, WFM_SIZE>& arr) {
        noise_floor_map.write_array(arr);
    }

    std::array<float, WFM_SIZE>& get_spectrum() {
        cfg.set_bit<reg::addr, 1>();
        wait_for_acquisition();

        if (sts.read<reg::avg_on_out>()) {
            float num_avg = float(get_num_average());
            for (unsigned int i=0; i<WFM_SIZE; i++)
                spectrum_data[i] = raw_data[i] / num_avg;
        } else {
            for (unsigned int i=0; i<WFM_SIZE; i++)
                spectrum_data[i] = raw_data[i];
        }
        cfg.clear_bit<reg::addr, 1>();
        return spectrum_data;
    }

    std::vector<float>& get_spectrum_decim(uint32_t decim_factor, uint32_t index_low, uint32_t index_high) {
        // Sanity checks
        if (index_high <= index_low || index_high >= WFM_SIZE) {
            spectrum_decim.resize(0);
            return spectrum_decim;
        }

        cfg.set_bit<reg::addr, 1>();
        uint32_t n_pts = (index_high - index_low)/decim_factor;
        spectrum_decim.resize(n_pts);
        wait_for_acquisition();

        if (sts.read<reg::avg_on_out>()) {
            float num_avg = float(get_num_average());

            for (unsigned int i=0; i<spectrum_decim.size(); i++)
                spectrum_decim[i] = raw_data[index_low + decim_factor * i] / num_avg;
        } else {
            for (unsigned int i=0; i<spectrum_decim.size(); i++)
                spectrum_decim[i] = raw_data[index_low + decim_factor * i];
        }

        cfg.clear_bit<reg::addr, 1>();
        return spectrum_decim;
    }

    void set_averaging(bool avg_on) {
        cfg.write_bit<reg::avg, 0>(avg_on);
    }

    uint32_t get_num_average()  {return sts.read<reg::n_avg>();}
    uint32_t get_peak_address() {return sts.read<reg::peak_address>();}
    uint32_t get_peak_maximum() {return sts.read<reg::peak_maximum>();}

    uint64_t get_counter() {
        uint64_t lsb = sts.read<reg::counter0>();
        uint64_t msb = sts.read<reg::counter1>();
        return lsb + (msb << 32);
    }

    void set_address_range(uint32_t address_low, uint32_t address_high) {
        cfg.write<reg::peak_address_low>(address_low);
        cfg.write<reg::peak_address_high>(address_high);
        cfg.write<reg::peak_address_reset>((address_low + WFM_SIZE - 1) % WFM_SIZE);
    }

    // Peak FIFO

    uint32_t get_fifo_occupancy() {
        return peak_fifo_map.read<FIFO_RDFO_OFF>();
    }

    void reset_fifo() {
        peak_fifo_map.write<FIFO_RDFR_OFF>(0x000000A5);
    }

    uint32_t read_fifo() {
        return peak_fifo_map.read<FIFO_RDFD_OFF>();
    }

    uint32_t get_fifo_length() {
        return (peak_fifo_map.read<FIFO_RLR_OFF>() & 0x3FFFFF) >> 2;
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
    Memory<mem::config>& cfg;
    Memory<mem::status>& sts;
    Memory<mem::spectrum>& spectrum_map;
    Memory<mem::demod>& demod_map;
    Memory<mem::peak_fifo>& peak_fifo_map;
    Memory<mem::noise_floor>& noise_floor_map;

    // Acquired data buffers
    float *raw_data;
    std::array<float, WFM_SIZE> spectrum_data;
    std::vector<float> spectrum_decim;
    std::vector<uint32_t> peak_fifo_data;

    DacRouter<prm::n_dac, prm::n_dac_bram> dac;

    // Internal functions
    void wait_for_acquisition() {
       do {} while (sts.read<reg::avg_ready>() == 0);
    }

}; // class Spectrum

#endif // __DRIVERS_SPECTRUM_HPP__
