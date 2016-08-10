/// Spectrum analyzer driver
///
/// (c) Koheron

#ifndef __DRIVERS_CORE_SPECTRUM_HPP__
#define __DRIVERS_CORE_SPECTRUM_HPP__

#include <drivers/lib/dev_mem.hpp>
#include <drivers/lib/fifo_reader.hpp>
#include <drivers/lib/dac_router.hpp>
#include <drivers/addresses.hpp>

#define SAMPLING_RATE 125E6
#define WFM_SIZE SPECTRUM_RANGE/sizeof(float)
#define FIFO_BUFF_SIZE 4096

constexpr std::array<std::array<uint32_t, 2>, N_DAC_BRAM_PARAM> dac_brams  = {{
    {DAC1_ADDR, DAC1_RANGE},
    {DAC2_ADDR, DAC2_RANGE},
    {DAC3_ADDR, DAC3_RANGE}
}};

class Spectrum
{
  public:
    Spectrum(Klib::DevMem& dvm_);

    void reset() {
        dvm.clear_bit(config_map, ADDR_OFF, 0);
        dvm.set_bit(config_map, ADDR_OFF, 0);
    }

    void set_n_avg_min(uint32_t n_avg_min);

    void set_period(uint32_t period) {
        set_dac_period(period, period);
        set_avg_period(period);
        reset();
    }

    void set_dac_period(uint32_t dac_period0, uint32_t dac_period1) {
        dvm.write32(config_map, DAC_PERIOD0_OFF, dac_period0 - 1);
        dvm.write32(config_map, DAC_PERIOD1_OFF, dac_period1 - 1);
    }

    void set_avg_period(uint32_t avg_period) {
        dvm.write32(config_map, AVG_PERIOD_OFF, avg_period - 1);
        dvm.write32(config_map, AVG_THRESHOLD_OFF, avg_period - 6);
    }

    void set_dac_buffer(uint32_t channel, const std::array<uint32_t, WFM_SIZE/2>& arr) {
        dac.set_data(channel, arr);
    }

    std::array<uint32_t, WFM_SIZE/2>& get_dac_buffer(uint32_t channel) {
        return dac.get_data<WFM_SIZE/2>(channel);
    }

    void reset_acquisition() {
        dvm.clear_bit(config_map, ADDR_OFF, 1);
        dvm.set_bit(config_map, ADDR_OFF, 1);
    }

    void set_scale_sch(uint32_t scale_sch) {
        // LSB at 1 for forward FFT
        dvm.write32(config_map, CFG_FFT_OFF, 1 + 2 * scale_sch);
    }

    void set_offset(uint32_t offset_real, uint32_t offset_imag) {
        dvm.write32(config_map, SUBSTRACT_MEAN_OFF, offset_real + 16384 * offset_imag);
    }

    #pragma tcp-server write_array arg{data} arg{len}
    void set_demod_buffer(const uint32_t *data, uint32_t len) {
        dvm.write_buff32(demod_map, 0, data, len);
    }

    #pragma tcp-server write_array arg{data} arg{len}
    void set_noise_floor_buffer(const uint32_t *data, uint32_t len) {
        dvm.write_buff32(noise_floor_map, 0, data, len);
    }

    std::array<float, WFM_SIZE>& get_spectrum();

    std::vector<float>& get_spectrum_decim(uint32_t decim_factor, uint32_t index_low, uint32_t index_high);

    void set_averaging(bool avg_status);

    uint32_t get_num_average()  {return dvm.read32(status_map, N_AVG_OFF);}
    uint32_t get_peak_address() {return dvm.read32(status_map, PEAK_ADDRESS_OFF);}
    uint32_t get_peak_maximum() {return dvm.read32(status_map, PEAK_MAXIMUM_OFF);}

    uint64_t get_counter() {
        uint64_t lsb = dvm.read32(status_map, COUNTER0_OFF);
        uint64_t msb = dvm.read32(status_map, COUNTER1_OFF);
        return lsb + (msb << 32);
    }

    void set_address_range(uint32_t address_low, uint32_t address_high);
    /// @acq_period Sleeping time between two acquisitions (us)
    void fifo_start_acquisition(uint32_t acq_period) {fifo.start_acquisition(acq_period);}
    void fifo_stop_acquisition()                     {fifo.stop_acquisition();}
    uint32_t get_peak_fifo_length()                  {return fifo.get_fifo_length();}
    uint32_t store_peak_fifo_data()                  {return fifo.get_buffer_length();}
    std::vector<uint32_t>& get_peak_fifo_data()      {return fifo.get_data();}
    bool fifo_get_acquire_status()                   {return fifo.get_acquire_status();}

  private:
    Klib::DevMem& dvm;

    Klib::MemMapID config_map;
    Klib::MemMapID status_map;
    Klib::MemMapID spectrum_map;
    Klib::MemMapID demod_map;
    Klib::MemMapID noise_floor_map;
    Klib::MemMapID peak_fifo_map;

    // Acquired data buffers
    float *raw_data;
    std::array<float, WFM_SIZE> spectrum_data;
    std::vector<float> spectrum_decim;
    FIFOReader<FIFO_BUFF_SIZE> fifo;
    DacRouter<N_DAC_PARAM, N_DAC_BRAM_PARAM> dac;

    // Internal functions
    void wait_for_acquisition() {
       do {} while (dvm.read32(status_map, AVG_READY_OFF) == 0);
    }
}; // class Spectrum

#endif // __DRIVERS_CORE_SPECTRUM_HPP__
