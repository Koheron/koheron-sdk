/// FFT driver
///
/// (c) Koheron

#ifndef __DRIVERS_FFT_HPP__
#define __DRIVERS_FFT_HPP__

#include <context.hpp>
#include <atomic>
#include <thread>
#include <chrono>
#include <mutex>

class FFT
{
  public:
    FFT(Context& ctx)
    : ctl(ctx.mm.get<mem::control>())
    , sts(ctx.mm.get<mem::status>())
    , psd_map(ctx.mm.get<mem::psd>())
    , demod_map(ctx.mm.get<mem::demod>())
    {
        std::array<uint32_t,prm::fft_size> demod_buffer;
        demod_buffer.fill(0xFFFF0000);
        set_scale_sch(0);
        set_demod_buffer(demod_buffer);
        ctl.set_bit<reg::psd_valid,0>();
        start_psd_acquisition();
    }

    uint32_t get_fft_size() {
        return prm::fft_size;
    }

    uint32_t get_cycle_index() {
        return sts.read<reg::cycle_index>();
    }

    //////////////////////////////////////
    // Power Spectral Density
    //////////////////////////////////////

    void set_scale_sch(uint32_t scale_sch) {
        // LSB at 1 for forward FFT
        ctl.write<reg::ctl_fft>(1 + (scale_sch << 1));
    }

    void set_offset(uint32_t offset_real, uint32_t offset_imag) {
        ctl.write<reg::substract_mean>(offset_real + (offset_imag << prm::adc_width));
    }

    void set_demod_buffer(const std::array<uint32_t, prm::fft_size>& arr) {
        demod_map.set_ptr(arr.data(), prm::fft_size, 0);
        demod_map.set_ptr(arr.data(), prm::fft_size, 1);
    }

    // Read averaged spectrum data
    std::array<float, prm::fft_size/2>& read_psd() {
        std::lock_guard<std::mutex> lock(mutex);
        return psd_buffer;
    }

    void start_psd_acquisition();

    //////////////////////////////////////
    // Direct Digital Synthesis
    //////////////////////////////////////

    void set_dds_freq(uint32_t channel, double freq_hz) {
        double factor = (uint64_t(1) << 32) / double(prm::adc_clk);
        ctl.write_reg(reg::phase_incr0 + 4 * channel, uint32_t(factor * freq_hz));
        dds_freq[channel] = freq_hz;
    }

    auto get_control_parameters() {
        return std::make_tuple(dds_freq[0], dds_freq[1]);
    }

  private:
    Memory<mem::control>& ctl;
    Memory<mem::status>& sts;
    Memory<mem::psd>& psd_map;
    Memory<mem::demod>& demod_map;

    std::array<double, 2> dds_freq = {{0.0, 0.0}};

    std::array<float, prm::fft_size/2> psd_buffer;
    std::thread psd_thread;
    std::mutex mutex;
    std::atomic<bool> psd_acquisition_started{false};
    std::atomic<uint32_t> acq_cycle_index{0};
    void psd_acquisition_thread();
};

inline void FFT::start_psd_acquisition() {
    if (! psd_acquisition_started) {
        psd_buffer.fill(0);
        psd_thread = std::thread{&FFT::psd_acquisition_thread, this};
        psd_thread.detach();
    }
}

inline void  FFT::psd_acquisition_thread()
{
    psd_acquisition_started = true;

    while (psd_acquisition_started) {
        uint32_t cycle_index = get_cycle_index();
        uint32_t previous_cycle_index = cycle_index;

        // Wait for
        while (cycle_index >= previous_cycle_index) {
            uint32_t sleep_time_ns = (prm::n_cycles - cycle_index) * prm::fft_size * 1e9 / prm::adc_clk;
            if (sleep_time_ns > 1'000'000) {
                std::this_thread::sleep_for(std::chrono::nanoseconds(sleep_time_ns));
            }
            previous_cycle_index = cycle_index;
            cycle_index = get_cycle_index();
        }
        {
            std::lock_guard<std::mutex> lock(mutex);
            psd_buffer = psd_map.read_array<float, prm::fft_size/2, 0>();
        }


        acq_cycle_index = get_cycle_index();
    }
}

#endif // __DRIVERS_FFT_HPP__
