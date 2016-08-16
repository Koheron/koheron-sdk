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

class Spectrum
{
  public:
    Spectrum(DevMem& dvm_);

    void reset() {
        cfg.clear_bit<ADDR_OFF, 0>();
        cfg.set_bit<ADDR_OFF, 0>();
    }

    void set_n_avg_min(uint32_t n_avg_min) {
        uint32_t n_avg_min_ = (n_avg_min < 2) ? 0 : n_avg_min-2;
        cfg.write<N_AVG_MIN_OFF>(n_avg_min_);
    }

    void set_period(uint32_t period) {
        set_dac_period(period, period);
        set_avg_period(period);
        reset();
    }

    void set_dac_period(uint32_t dac_period0, uint32_t dac_period1) {
        cfg.write<DAC_PERIOD0_OFF>(dac_period0 - 1);
        cfg.write<DAC_PERIOD1_OFF>(dac_period1 - 1);
    }

    void set_avg_period(uint32_t avg_period) {
        cfg.write<AVG_PERIOD_OFF>(avg_period - 1);
        cfg.write<AVG_THRESHOLD_OFF>(avg_period - 6);
    }

    void set_dac_buffer(uint32_t channel, const std::array<uint32_t, WFM_SIZE/2>& arr) {
        dac.set_data(channel, arr);
    }

    std::array<uint32_t, WFM_SIZE/2>& get_dac_buffer(uint32_t channel) {
        return dac.get_data<WFM_SIZE/2>(channel);
    }

    void reset_acquisition() {
        cfg.clear_bit<ADDR_OFF, 1>();
        cfg.set_bit<ADDR_OFF, 1>();
    }

    void set_scale_sch(uint32_t scale_sch) {
        // LSB at 1 for forward FFT
        cfg.write<CFG_FFT_OFF>(1 + 2 * scale_sch);
    }

    void set_offset(uint32_t offset_real, uint32_t offset_imag) {
        cfg.write<SUBSTRACT_MEAN_OFF>(offset_real + 16384 * offset_imag);
    }

    void set_demod_buffer(const std::array<uint32_t, WFM_SIZE>& arr) {
        demod_map.write_array(arr);
    }

    void set_noise_floor_buffer(const std::array<uint32_t, WFM_SIZE>& arr) {
        noise_floor_map.write_array(arr);
    }

    std::array<float, WFM_SIZE>& get_spectrum();

    std::vector<float>& get_spectrum_decim(uint32_t decim_factor, uint32_t index_low, uint32_t index_high);

    void set_averaging(bool avg_status);

    uint32_t get_num_average()  {return sts.read<N_AVG_OFF>();}
    uint32_t get_peak_address() {return sts.read<PEAK_ADDRESS_OFF>();}
    uint32_t get_peak_maximum() {return sts.read<PEAK_MAXIMUM_OFF>();}

    uint64_t get_counter() {
        uint64_t lsb = sts.read<COUNTER0_OFF>();
        uint64_t msb = sts.read<COUNTER1_OFF>();
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
    DevMem& dvm;

    MemoryMap& cfg;
    MemoryMap& sts;
    MemoryMap& spectrum_map;
    MemoryMap& demod_map;
    MemoryMap& noise_floor_map;
    MemoryMap& peak_fifo_map;

    // Acquired data buffers
    float *raw_data;
    std::array<float, WFM_SIZE> spectrum_data;
    std::vector<float> spectrum_decim;
    FIFOReader<FIFO_BUFF_SIZE> fifo;
    DacRouter<N_DAC_PARAM, N_DAC_BRAM_PARAM> dac;

    // Internal functions
    void wait_for_acquisition() {
       do {} while (sts.read<AVG_READY_OFF>() == 0);
    }
}; // class Spectrum

#endif // __DRIVERS_CORE_SPECTRUM_HPP__
