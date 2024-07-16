// SPDX-License-Identifier: MIT
// Copyright (c) 2021-2022 Thomas Vanderbruggen <th.vanderbruggen@gmail.com>

#ifndef SCICPP_PLOTS_HIST
#define SCICPP_PLOTS_HIST

#include "scicpp/core/histogram.hpp"
#include "scicpp/core/units/quantity.hpp"

#include <cstdlib>
#include <sciplot/sciplot.hpp>
#include <string>
#include <utility>
#include <vector>

namespace scicpp::plots {

enum HistType : int { BAR, BARSTACKED, STEP, STEPFILLED };

namespace detail {

template <typename HistVector>
struct histplot : sciplot::Plot2D {
  public:
    template <
        typename Array,
        typename RepTp = units::representation_t<typename Array::value_type>>
    histplot(HistVector &&hist, const Array &bins)
        : m_hist(std::move(hist)),
          m_bins(sciplot::Vec(reinterpret_cast<const RepTp *>(bins.data()),
                              bins.size())) {
        xrange(m_bins[0], m_bins[m_bins.size() - 1]);
        redraw();
    }

    auto color(const std::string &fill_color) {
        m_fill_color = fill_color;
        redraw();
        return *this;
    }

    auto log(bool logscale) {
        m_logscale = logscale;

        if (m_logscale) {
            ytics().logscale(10);
        }

        return *this;
    }

    auto histtype(HistType hist_type) {
        m_hist_type = hist_type;
        redraw();
        return *this;
    }

    auto rwidth(double rwidth) {
        m_rwidth = rwidth;
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
    std::string m_fill_color = "blue";
    HistVector m_hist;
    sciplot::Vec m_bins;
    bool m_logscale = false;
    HistType m_hist_type = BAR;
    double m_rwidth = 1.0;

    void redraw() {
        clear();

        using HistTp = typename HistVector::value_type;
        using RepTp = units::representation_t<HistTp>;

        if (m_hist_type == BAR) {
            if constexpr (units::is_quantity_v<HistTp>) {
                drawBoxes(
                    m_bins,
                    sciplot::Vec(reinterpret_cast<const RepTp *>(m_hist.data()),
                                 m_hist.size()))
                    .fillColor(m_fill_color)
                    .labelNone();
            } else {
                drawBoxes(m_bins, m_hist).fillColor(m_fill_color).labelNone();
            }
            boxWidthRelative(m_rwidth);
        } else if (m_hist_type == STEPFILLED) {
            if constexpr (units::is_quantity_v<HistTp>) {
                drawStepsFilled(
                    m_bins,
                    sciplot::Vec(reinterpret_cast<const RepTp *>(m_hist.data()),
                                 m_hist.size()))
                    .fillColor(m_fill_color)
                    .labelNone();
            } else {
                drawStepsFilled(m_bins, m_hist)
                    .fillColor(m_fill_color)
                    .labelNone();
            }
        } else {
            // TODO: BARSTACKED, STEP
        }

        border().right().top();

        if (m_logscale) {
            ytics().logscale(10);
        }
    }
}; // class histplot

} // namespace detail

template <bool density = false,
          bool use_uniform_bins = false,
          typename Array,
          typename T = typename Array::value_type>
auto hist(const Array &x, const std::vector<T> &bins) {
    return detail::histplot(
        stats::histogram<density, use_uniform_bins>(x, bins), bins);
}

template <stats::BinEdgesMethod method, bool density = false, typename Array>
auto hist(const Array &x) {
    auto [hist_res, bins] = stats::histogram<method, density>(x);
    return detail::histplot(std::move(hist_res), std::move(bins));
}

template <bool density = false, typename Array>
auto hist(const Array &x, std::size_t nbins = 10) {
    auto [hist_res, bins] = stats::histogram<density>(x, nbins);
    return detail::histplot(std::move(hist_res), std::move(bins));
}

} // namespace scicpp::plots

#endif // SCICPP_PLOTS_HIST