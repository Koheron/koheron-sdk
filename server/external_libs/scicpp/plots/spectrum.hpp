// SPDX-License-Identifier: MIT
// Copyright (c) 2022 Thomas Vanderbruggen <th.vanderbruggen@gmail.com>

#ifndef SCICPP_PLOTS_CSD
#define SCICPP_PLOTS_CSD

#include "scicpp/core/maths.hpp"
#include "scicpp/signal/spectral.hpp"

#include <cstdint>
#include <sciplot/sciplot.hpp>
#include <string>
#include <utility>

namespace scicpp::plots {

namespace detail {

template <typename FreqArray, typename PowerArray>
struct csdplot : sciplot::Plot2D {
    using FreqTp = typename FreqArray::value_type;
    using PowerTp = typename PowerArray::value_type;
    using FreqRepTp = units::representation_t<FreqTp>;
    using PowerRepTp = units::representation_t<PowerTp>;

  public:
    csdplot(FreqArray &&freqs, PowerArray &&power)
        : m_freqs(sciplot::Vec(
              reinterpret_cast<const FreqRepTp *>(freqs.data()), freqs.size())),
          m_power(
              sciplot::Vec(reinterpret_cast<const PowerRepTp *>(power.data()),
                           power.size())) {
        redraw();
    }

    auto color(const std::string &color) {
        m_color = color;
        redraw();
        return *this;
    }

    auto display_grid(bool display_grid) {
        m_display_grid = display_grid;
        redraw();
        return *this;
    }

    auto canvas() {
        sciplot::Figure fig = {{*this}};
        sciplot::Canvas canvas = {{fig}};
        return canvas;
    }

    void show(std::size_t width = 750, std::size_t height = 600) {
        auto c = canvas();
        c.size(width, height);
        c.show();
    }

  private:
    std::string m_color = "blue";
    bool m_display_grid = true;
    sciplot::Vec m_freqs;
    sciplot::Vec m_power;

    void redraw() {
        clear();
        drawCurve(m_freqs, m_power).lineColor(m_color).labelNone();

        if (m_display_grid) {
            grid().lineType(-1).lineWidth(2).show();
        } else {
            grid().hide();
        }
        border().right().top();
    }

}; // class csdplot

} // namespace detail

enum SpectrumPlotScale : int { LINEAR, DECIBEL };

template <signal::SpectrumScaling scaling = signal::DENSITY,
          SpectrumPlotScale plot_scale = DECIBEL,
          typename Array1,
          typename Array2,
          typename T = double>
auto csd(signal::Spectrum<T> spec, const Array1 &x, const Array2 &y) {
    using namespace operators;
    auto [f, Pxy] = spec.template csd<scaling>(x, y);

    if constexpr (plot_scale == LINEAR) {
        return detail::csdplot(std::move(f), norm(std::move(Pxy)));
    } else { // plot_scale == DECIBEL
        using PxyTp = typename decltype(Pxy)::value_type;
        return detail::csdplot(std::move(f),
                               T{10} * log10(norm(std::move(Pxy) / PxyTp(1))));
    }
}

template <signal::SpectrumScaling scaling = signal::DENSITY,
          SpectrumPlotScale plot_scale = DECIBEL,
          typename Array,
          typename T = double>
auto psd(signal::Spectrum<T> spec, const Array &x) {
    using namespace operators;

    auto [f, Pxx] = spec.template welch<scaling>(x);

    if constexpr (plot_scale == LINEAR) {
        return detail::csdplot(std::move(f), std::move(Pxx));
    } else { // plot_scale == DECIBEL
        using PxxTp = typename decltype(Pxx)::value_type;
        return detail::csdplot(std::move(f),
                               T{10} * log10(std::move(Pxx) / PxxTp(1)));
    }
}

template <typename Array1, typename Array2, typename T = double>
auto cohere(signal::Spectrum<T> spec, const Array1 &x, const Array2 &y) {
    using namespace operators;
    auto [f, Cxy] = spec.coherence(x, y);
    return detail::csdplot(std::move(f), std::move(Cxy));
}

} // namespace scicpp::plots

#endif // SCICPP_PLOTS_CSD