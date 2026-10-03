#pragma once

#include <scicpp/core.hpp>
#include <scicpp/signal/windows.hpp>
#include <unsupported/Eigen/FFT>
#include <algorithm>
#include <array>
#include <cassert>
#include <complex>
#include <future>
#include <vector>

namespace phase_noise {

// Acquisition owns this estimator. Two independent plans/buffer sets survive
// across captures; one background task processes alternate Welch segments.
template<std::size_t FftSize>
class WelchSpectrum {
    static_assert(FftSize > 1 && FftSize % 2 == 0);
    static constexpr std::size_t bins = FftSize / 2 + 1;
    std::vector<float> window = scicpp::signal::windows::hann<float>(FftSize);
    struct Workspace {
        std::vector<float> weighted = std::vector<float>(FftSize);
        std::vector<std::complex<float>> transformed = std::vector<std::complex<float>>(bins);
        std::vector<float> power = std::vector<float>(bins);
        Eigen::FFT<float> fft;
        Workspace() { fft.SetFlag(Eigen::FFT<float>::HalfSpectrum); }
    };
    std::array<Workspace, 2> workspaces;
    double window_power = 0.0;

  public:
    WelchSpectrum() {
        for (const auto value : window)
            window_power += double(value) * double(value);
    }

    template<class Array>
    auto density(const Array& input, scicpp::units::frequency<float> fs) {
        using Phase = typename Array::value_type;
        using Density = scicpp::units::quantity_divide<
            scicpp::units::quantity_multiply<Phase, Phase>,
            scicpp::units::frequency<float>>;
        assert(input.size() >= FftSize && fs.eval() > 0.0f);
        const auto segments = 1 + (input.size() - FftSize) / (FftSize / 2);
        for (auto& workspace : workspaces)
            std::fill(workspace.power.begin(), workspace.power.end(), 0.0f);
        const auto process = [&](std::size_t worker) {
            auto& workspace = workspaces[worker];
            for (std::size_t segment = worker; segment < segments; segment += 2) {
                const auto offset = segment * (FftSize / 2);
                double mean = 0.0;
                for (std::size_t i = 0; i < FftSize; ++i)
                    mean += double(input[offset + i].eval());
                const float center = float(mean / double(FftSize));
                for (std::size_t i = 0; i < FftSize; ++i)
                    workspace.weighted[i] = (input[offset + i].eval() - center) * window[i];
                workspace.fft.fwd(workspace.transformed.data(), workspace.weighted.data(), FftSize);
                for (std::size_t i = 0; i < bins; ++i)
                    workspace.power[i] += std::norm(workspace.transformed[i]);
            }
        };
        std::future<void> background;
        if (segments > 1) background = std::async(std::launch::async, process, 1);
        process(0);
        if (background.valid()) background.get();
        const float scale = float(1.0 / (double(segments) * double(fs.eval()) * window_power));
        std::vector<Density> result(bins);
        for (std::size_t i = 0; i < bins; ++i)
            result[i] = Density{(workspaces[0].power[i] + workspaces[1].power[i]) * scale *
                (i == 0 || i == bins - 1 ? 1.0f : 2.0f)};
        return result;
    }
};

} // namespace phase_noise
