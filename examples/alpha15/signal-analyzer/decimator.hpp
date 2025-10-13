/// Pulse driver
///
/// (c) Koheron

#ifndef __ALPHA15_SIGNAL_ANALYZER_DECIMATOR_HPP__
#define __ALPHA15_SIGNAL_ANALYZER_DECIMATOR_HPP__

#include "./fifo_spectral_analyzer.hpp"

#include <cstdint>

class Context;

class Decimator
{
  public:
    Decimator(Context& ctx_);
    void set_fft_window(uint32_t window_id);

    auto get_control_parameters() const {
        return std::tuple{
            analyzer0.fs,
            analyzer1.fs,
            analyzer0.fifo_transfer_duration,
            analyzer1.fifo_transfer_duration,
            FifoCfg0::cic_rate,
            FifoCfg1::cic_rate,
            FifoCfg0::n_pts
        };
    }

    auto spectral_density0() const {
        return analyzer0.spectral_density();
    }

    auto spectral_density1() const {
        return analyzer1.spectral_density();
    }

  private:
    Context& ctx;

    static constexpr uint32_t n_segs = 4;

    struct FifoCfg0 {
        static constexpr uint32_t    fifo_idx   = 0;
        static constexpr std::size_t fifo_mem   = mem::adc_fifo0;
        static constexpr uint32_t    n_fifo     = 4 * prm::fifo_depth;
        static constexpr uint32_t    n_acq_max  = prm::fifo_depth / 2;
        static constexpr uint32_t    n_pts      = n_fifo / n_segs;
        static constexpr std::size_t navg       = 16;
        static constexpr std::size_t cic_rate   = 16;
    };

    FifoSpectralAnalyzer<FifoCfg0> analyzer0;

    struct FifoCfg1 {
        static constexpr uint32_t    fifo_idx   = 1;
        static constexpr std::size_t fifo_mem   = mem::adc_fifo1;
        static constexpr uint32_t    n_fifo     = 4 * prm::fifo_depth;
        static constexpr uint32_t    n_acq_max  = prm::fifo_depth / 2;
        static constexpr uint32_t    n_pts      = n_fifo / n_segs;
        static constexpr std::size_t navg       = 16;
        static constexpr std::size_t cic_rate   = 256;
    };

    FifoSpectralAnalyzer<FifoCfg1> analyzer1;

    void start_acquisition();
}; // Decimator

#endif // __ALPHA15_SIGNAL_ANALYZER_DECIMATOR_HPP__
