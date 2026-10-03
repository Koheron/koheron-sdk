#pragma once

#include <scicpp/core.hpp>
#include <scicpp/signal/windows.hpp>
#include "server/external_libs/pffft/pffft.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <future>
#include <memory>
#include <vector>

namespace phase_noise {

// Acquisition owns this estimator. A read-only plan and two buffer sets survive
// across captures; one background task processes alternate Welch segments.
template<std::size_t FftSize>
class WelchSpectrum {
    static_assert(FftSize >= 32 && (FftSize & (FftSize - 1)) == 0);
    static constexpr std::size_t bins = FftSize / 2 + 1;
    std::vector<float> window = scicpp::signal::windows::hann<float>(FftSize);
    struct AlignedFree {
        void operator()(float* pointer) const { pffft_aligned_free(pointer); }
    };
    using Buffer = std::unique_ptr<float, AlignedFree>;
    static Buffer make_buffer() {
        Buffer result{static_cast<float*>(pffft_aligned_malloc(FftSize * sizeof(float)))};
        assert(result);
        return result;
    }
    std::unique_ptr<PFFFT_Setup, decltype(&pffft_destroy_setup)> setup{
        pffft_new_setup(int(FftSize), PFFFT_REAL), pffft_destroy_setup};
    struct Workspace {
        Buffer weighted = make_buffer();
        Buffer transformed = make_buffer();
        Buffer scratch = make_buffer();
        std::vector<float> power = std::vector<float>(bins);
    };
    std::array<Workspace, 2> workspaces;
    double window_power = 0.0;

  public:
    WelchSpectrum() {
        assert(setup);
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
                    workspace.weighted.get()[i] = (input[offset + i].eval() - center) * window[i];
                pffft_transform_ordered(setup.get(), workspace.weighted.get(),
                    workspace.transformed.get(), workspace.scratch.get(), PFFFT_FORWARD);
                const auto* transformed = workspace.transformed.get();
                // Real transforms pack DC and Nyquist into the first pair.
                workspace.power[0] += transformed[0] * transformed[0];
                workspace.power[bins - 1] += transformed[1] * transformed[1];
                for (std::size_t i = 1; i < bins - 1; ++i)
                    workspace.power[i] += transformed[2 * i] * transformed[2 * i] +
                                          transformed[2 * i + 1] * transformed[2 * i + 1];
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
