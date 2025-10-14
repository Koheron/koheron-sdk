/// PhaseNoiseAnalyzer driver
///
/// (c) Koheron

#ifndef __PHASE_NOISE_ANALYZER_HPP__
#define __PHASE_NOISE_ANALYZER_HPP__

#include <array>
#include <atomic>
#include <cstdint>
#include <shared_mutex>
#include <tuple>
#include <vector>
#include <scicpp/core.hpp>
#include <scicpp/signal.hpp>

#include "server/hardware/memory_manager.hpp"

#include "./dds.hpp"
#include "./moving_averager.hpp"

namespace rt { class ConfigManager; }
class DmaS2MM;
class Ltc2157;
class Dds;

class PhaseNoiseAnalyzer
{
    using Phase = scicpp::units::radian<float>;
    using Time = scicpp::units::time<float>;
    using Frequency = scicpp::units::frequency<float>;
    using PhaseNoiseDensity = scicpp::units::quantity_divide<
                scicpp::units::quantity_multiply<Phase, Phase>,
                Frequency>;

    static constexpr uint32_t fft_size = 32768;
    static constexpr uint32_t data_size = 2 * fft_size;
    static constexpr uint32_t read_offset = (prm::n_pts - data_size) / 2; // Do use the first transfered points
    static constexpr auto calib_factor = 4.196f * scicpp::pi<Phase> / 8192.0f;

    using PhaseDataArray = std::array<Phase, data_size>;
    using PhaseNoiseDensityVector = std::vector<PhaseNoiseDensity>;

  public:
    PhaseNoiseAnalyzer();

    void save_config();
    void set_local_oscillator(uint32_t channel, double freq_hz);
    void set_cic_rate(uint32_t rate);
    void set_channel(uint32_t chan);
    void set_fft_navg(uint32_t n_avg);
    void set_analyzer_mode(uint32_t mode);
    void set_interferometer_delay(float delay_s);

    auto get_parameters() {
        return std::tuple{
            fft_size / 2,
            fs,
            channel,
            cic_rate,
            fft_navg,
            dds.get_dds_freq(0),
            dds.get_dds_freq(1),
            analyzer_mode,
            interferometer_delay
        };
    }

    double get_carrier_power(uint32_t navg); // Carrier power in dBm

    auto get_jitter() {
        return std::tuple{
            phase_jitter,
            time_jitter,
            f_lo_used,
            f_hi_used
        };
    }

    PhaseDataArray get_phase() const;
    PhaseNoiseDensityVector get_phase_noise() const;

  private:
    rt::ConfigManager& cfg;
    DmaS2MM& dma;
    Ltc2157& ltc2157;
    Dds& dds;
    hw::Memory<mem::control>& ctl;
    hw::Memory<mem::status>& sts;

    uint32_t channel;
    uint32_t fft_navg;
    uint32_t cic_rate;
    std::atomic<int32_t> dirty_cnt = 0;
    Frequency fs_adc, fs;
    Time dma_transfer_duration;

    std::mutex dma_mtx; // Guard DMA transfer
    mutable std::shared_mutex data_mtx; // protects phase & phase_noise

    // Data acquisition thread
    std::thread acq_thread;
    std::atomic<bool> acquisition_started{false};

    PhaseDataArray phase;

    // Spectrum analyzer
    scicpp::signal::Spectrum<float> spectrum;
    PhaseNoiseDensityVector phase_noise;
    MovingAverager<PhaseNoiseDensity> averager;

    // Jitter (integrated noise)
    Phase phase_jitter{0.0f};
    Time time_jitter{0.0f};
    Frequency f_lo_used{0.0f}; // Integration interval start
    Frequency f_hi_used{0.0f}; // Integration interval end

    // Laser phase noise
    enum AnalyzerMode: uint32_t {
        RF,   // Return the RF signal phase noise
        LASER // Return the laser phase noise (compensate for interferometer response)
    };

    uint32_t analyzer_mode = AnalyzerMode::RF;
    Time interferometer_delay{0.0f};
    std::array<float, 1 + fft_size / 2> interferometer_tf{}; // Interferometer transfer function

    // Carrier power
    scicpp::units::dimensionless<double> conv_factor_dBm;
    std::array<scicpp::units::electric_potential<double>, 2> vrange;

    // ----------------- Private functions

    void load_config();
    void reset_phase_unwrapper();
    void kick_dma();
    auto read_dma();
    void update_interferometer_transfer_function();
    void set_power_conversion_factor();
    auto compute_phase_noise(PhaseDataArray& new_phase);
    auto compute_jitter(const PhaseNoiseDensityVector& new_pn);
    void acquisition_thread();
    void start_acquisition();
};

#endif // __PHASE_NOISE_ANALYZER_HPP__
