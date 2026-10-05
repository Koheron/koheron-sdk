#pragma once

#include "server/external_libs/pffft/pffft.h"
#include <scicpp/signal.hpp>
#include <algorithm>
#include <cassert>
#include <complex>
#include <future>
#include <memory>
#include <vector>

namespace phase_noise {

// A fixed-duration periodogram. Plans, Hann weights and working buffers belong
// to the acquisition thread and survive sample-rate and channel changes.
// PFFFT's complex transform supports 30000 samples; its real transform does
// not. Transform each real channel separately to avoid cancellation when
// recovering a quiet channel from a packed two-real-input transform.
class SingleWindowSpectrum {
    struct AlignedFree {
        void operator()(float* pointer) const { pffft_aligned_free(pointer); }
    };
    using Buffer = std::unique_ptr<float[], AlignedFree>;
    static Buffer make_buffer(std::size_t count) {
        Buffer result{static_cast<float*>(pffft_aligned_malloc(count * sizeof(float)))};
        assert(result);
        return result;
    }
    const std::size_t size;
    const std::vector<float> window;
    const float window_power;
    std::unique_ptr<PFFFT_Setup, decltype(&pffft_destroy_setup)> setup;
    struct Workspace {
        Buffer input, transformed, scratch;
        std::vector<std::complex<float>> fallback_output;
    };
    Workspace x_workspace, y_workspace;
    Eigen::FFT<float> fallback;

    Workspace make_workspace() const {
        Workspace result{
            make_buffer(setup ? 2 * size : size),
            setup ? make_buffer(2 * size) : Buffer{},
            setup ? make_buffer(2 * size) : Buffer{},
            std::vector<std::complex<float>>(setup ? 0 : size / 2 + 1)};
        std::fill_n(result.input.get(), setup ? 2 * size : size, 0.0f);
        return result;
    }

    template<class Value>
    static float scalar(Value value) {
        if constexpr (scicpp::units::is_quantity_v<Value>) return value.eval();
        else return value;
    }

    template<class Array>
    void transform(const Array& phase, Workspace& workspace) {
        assert(phase.size() == size);
        const auto mean = scicpp::stats::mean(phase);
        if (setup) {
            for (std::size_t i = 0; i < size; ++i)
                workspace.input[2 * i] = scalar((phase[i] - mean) * window[i]);
            // Imaginary inputs stay zero: transforms never modify input.
            pffft_transform_ordered(setup.get(), workspace.input.get(), workspace.transformed.get(),
                                    workspace.scratch.get(), PFFFT_FORWARD);
        } else {
            for (std::size_t i = 0; i < size; ++i)
                workspace.input[i] = scalar((phase[i] - mean) * window[i]);
            fallback.fwd(workspace.fallback_output.data(), workspace.input.get(), int(size));
        }
    }

    std::complex<float> bin(const Workspace& workspace, std::size_t k) const {
        if (setup) return {workspace.transformed[2 * k], workspace.transformed[2 * k + 1]};
        return workspace.fallback_output[k];
    }

    template<class Array>
    using Density = scicpp::units::quantity_divide<
        scicpp::units::quantity_multiply<typename Array::value_type, typename Array::value_type>,
        scicpp::units::frequency<float>>;

  public:
    explicit SingleWindowSpectrum(std::size_t samples)
    : size(samples), window(scicpp::signal::windows::hann<float>(samples)),
      window_power(scicpp::signal::windows::s2(window)),
      setup(pffft_is_valid_size(int(samples), PFFFT_COMPLEX) ?
          pffft_new_setup(int(samples), PFFFT_COMPLEX) : nullptr, pffft_destroy_setup),
      x_workspace(make_workspace()), y_workspace(make_workspace()) {
        assert(size > 1);
        fallback.SetFlag(Eigen::FFT<float>::HalfSpectrum);
    }

    template<class Array>
    auto density(const Array& phase, scicpp::units::frequency<float> fs) {
        assert(fs.eval() > 0.0f);
        transform(phase, x_workspace);
        const float denominator = fs.eval() * window_power;
        std::vector<Density<Array>> result(size / 2 + 1);
        for (std::size_t k = 0; k < result.size(); ++k) {
            const float sides = k == 0 || (size % 2 == 0 && k == size / 2) ? 1.0f : 2.0f;
            result[k] = Density<Array>{sides * std::norm(bin(x_workspace, k)) / denominator};
        }
        return result;
    }

    template<class Array>
    auto cross_density(const Array& x, const Array& y, scicpp::units::frequency<float> fs) {
        assert(fs.eval() > 0.0f);
        // PFFFT plans are read-only. Its two workspaces can run concurrently;
        // the small Eigen fallback retains one mutable plan and runs serially.
        std::future<void> background;
        if (setup) background = std::async(std::launch::async, [&] { transform(y, y_workspace); });
        transform(x, x_workspace);
        if (background.valid()) background.get();
        else transform(y, y_workspace);
        const float denominator = fs.eval() * window_power;
        std::vector<std::complex<Density<Array>>> result(size / 2 + 1);
        for (std::size_t k = 0; k < result.size(); ++k) {
            const float sides = k == 0 || (size % 2 == 0 && k == size / 2) ? 1.0f : 2.0f;
            const auto value = sides * (std::conj(bin(x_workspace, k)) * bin(y_workspace, k)) / denominator;
            result[k] = {Density<Array>{value.real()}, Density<Array>{value.imag()}};
        }
        return result;
    }
};

} // namespace phase_noise
