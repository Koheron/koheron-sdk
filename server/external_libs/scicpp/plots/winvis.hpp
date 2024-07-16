// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Thomas Vanderbruggen <th.vanderbruggen@gmail.com>

// Window visualization tool
// See: https://fr.mathworks.com/matlabcentral/answers/84002-from-where-can-i-read-about-the-functioning-of-the-wvtool

#ifndef SCICPP_PLOTS_WINVIS
#define SCICPP_PLOTS_WINVIS

#include "scicpp/core/functional.hpp"
#include "scicpp/core/manips.hpp"
#include "scicpp/core/maths.hpp"
#include "scicpp/core/meta.hpp"
#include "scicpp/core/numeric.hpp"
#include "scicpp/core/range.hpp"
#include "scicpp/plots/plot.hpp"
#include "scicpp/signal/fft.hpp"
#include "scicpp/signal/windows.hpp"

#include <sciplot/sciplot.hpp>
#include <string>
#include <tuple>
#include <utility>

namespace scicpp::plots {

namespace detail {

template <typename Array>
auto window_spectrum_db(const Array &window) {
    using T = typename Array::value_type;
    using namespace operators;

    // Add zero padding on both sides of the window before computing the FFT
    const auto padding = zeros<T>(5 * window.size());
    auto spec = norm(signal::rfft(padding | window | padding));
    return 10.0 * log10(std::move(spec) / spec[0]);
}

// Mainlobe width at 3 dB
template <typename Freq, typename Array>
auto mainlobe_width(const Freq &f, const Array &winfft) {
    using T = typename Array::value_type;
    using namespace operators;

    const auto idx = std::size_t(argmin(winfft + T(3), filters::positive));
    return T(4) * f[idx]; // Factor 4 to match Matlab wvtool result
}

} // namespace detail

template <typename... Arrays, meta::enable_if_iterable<Arrays...> = 0>
void winvis(const std::tuple<Arrays...> &windows) {
    using Array0 = std::tuple_element_t<0, std::tuple<Arrays...>>;
    using T = typename Array0::value_type;

    const auto winsize = std::get<0>(windows).size();
    scicpp_require(std::apply(
        [=](auto... w) { return ((w.size() == winsize) && ...); }, windows));

    const auto x = linspace(T(0), T(winsize), winsize);

    auto plot_win = plot(x, windows);
    plot_win.xrange(T(0), T(winsize));
    plot_win.xlabel("Samples");
    plot_win.ylabel("Amplitude");

    const auto f = signal::rfftfreq<T>(11 * winsize);
    const auto win_ffts = std::apply(
        [](const auto &...wins) {
            return std::tuple{detail::window_spectrum_db(wins)...};
        },
        windows);

    const auto widths = std::apply(
        [&](const auto &...winffts) {
            return std::tuple{detail::mainlobe_width(f, winffts)...};
        },
        win_ffts);

    std::string width_str("Mainlobe width (3 dB): ");

    std::apply(
        [&](auto arg0, auto... args) {
            auto append_width = [&](auto width) {
                width_str += ", " + std::to_string(width);
            };

            width_str += "[" + std::to_string(arg0);
            (append_width(args), ...);
            width_str += "]";
        },
        widths);

    auto plot_fft = plot(f, win_ffts);
    plot_fft.xrange(f[0], f.back());
    plot_fft.yrange(-200.0, 3.0);
    plot_fft.xlabel("Nyquist frequency");
    plot_fft.ylabel("Magnitude (dB)");

    sciplot::Figure fig = {{plot_win, plot_fft}};
    fig.title(width_str);
    sciplot::Canvas canvas = {{fig}};
    canvas.size(1100, 500);
    canvas.show();
}

template <std::size_t N = 128, std::size_t Nwins>
void winvis(std::array<signal::windows::Window, Nwins> wins) {
    winvis(std::apply(
        [=](auto... windows) {
            return std::tuple{
                signal::windows::get_window<double>(windows, N)...};
        },
        wins));
}

template <std::size_t N = 128, typename... Windows>
void winvis(Windows... wins) {
    winvis<N>(std::array{wins...});
}

} // namespace scicpp::plots

#endif // SCICPP_PLOTS_WINVIS