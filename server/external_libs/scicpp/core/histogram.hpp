// SPDX-License-Identifier: MIT
// Copyright (c) 2021 Thomas Vanderbruggen <th.vanderbruggen@gmail.com>

#ifndef SCICPP_CORE_HISTOGRAM
#define SCICPP_CORE_HISTOGRAM

#include "scicpp/core/constants.hpp"
#include "scicpp/core/equal.hpp"
#include "scicpp/core/macros.hpp"
#include "scicpp/core/maths.hpp"
#include "scicpp/core/numeric.hpp"
#include "scicpp/core/print.hpp"
#include "scicpp/core/range.hpp"
#include "scicpp/core/stats.hpp"
#include "scicpp/core/units/quantity.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <type_traits>
#include <utility>
#include <vector>

namespace scicpp::stats {

//---------------------------------------------------------------------------------
// histogram_bin_edges
//---------------------------------------------------------------------------------

enum class BinEdgesMethod : int { SCOTT, SQRT, RICE, STURGES, FD, DOANE, AUTO };

namespace detail {

template <BinEdgesMethod method, typename Array>
auto scicpp_pure bin_width(const Array &x) {
    scicpp_require(!x.empty());

    using T = typename Array::value_type;
    using ret_t = std::conditional_t<std::is_integral_v<T>, double, T>;
    using raw_t = typename units::representation_t<ret_t>;

    if constexpr (method == BinEdgesMethod::SQRT) {
        return ptp(x) / sqrt(x.size());
    } else if constexpr (method == BinEdgesMethod::SCOTT) {
        return cbrt(raw_t{24} * sqrt(pi<raw_t>) / raw_t(x.size())) * std(x);
    } else if constexpr (method == BinEdgesMethod::RICE) {
        return raw_t{0.5} * ptp(x) / cbrt(x.size());
    } else if constexpr (method == BinEdgesMethod::STURGES) {
        return ptp(x) / (log2(x.size()) + 1.0);
    } else if constexpr (method == BinEdgesMethod::FD) {
        // Freedman-Diaconis histogram bin estimator.
        return raw_t{2} * iqr(x) / cbrt(x.size());
    } else if constexpr (method == BinEdgesMethod::DOANE) {
        if (x.size() <= 2) {
            return ret_t{0};
        }

        const auto sg1 = sqrt(raw_t{6} * raw_t(x.size() - 2) /
                              raw_t((x.size() + 1) * (x.size() + 3)));
        const auto g1 = units::value(skew(x));

        if (unlikely(std::isnan(g1))) {
            return ret_t{0};
        }

        return ptp(x) / (raw_t{1} + log2(x.size()) +
                         log2(raw_t{1} + absolute(g1) / sg1));
    } else { // AUTO
        const auto fd_bw = bin_width<BinEdgesMethod::FD>(x);
        const auto sturges_bw = bin_width<BinEdgesMethod::STURGES>(x);

        if (units::fpclassify(fd_bw) == FP_ZERO) {
            return sturges_bw;
        }

        return units::fmin(fd_bw, sturges_bw);
    }
}

template <typename Array>
auto scicpp_pure outer_edges(const Array &x) noexcept {
    using T = typename Array::value_type;
    using RetTp = std::conditional_t<std::is_integral_v<T>, double, T>;

    if (unlikely(x.empty())) {
        return std::make_pair(RetTp{0}, RetTp{1});
    }

    auto first_edge = RetTp(amin(x));
    auto last_edge = RetTp(amax(x));

    if (almost_equal(first_edge, last_edge)) {
        first_edge -= RetTp{0.5};
        last_edge += RetTp{0.5};
    }

    return std::make_pair(first_edge, last_edge);
}

} // namespace detail

template <BinEdgesMethod method, typename Array>
auto histogram_bin_edges(const Array &x) {
    using T = typename Array::value_type;
    using RetTp = std::conditional_t<std::is_integral_v<T>, double, T>;

    if (unlikely(x.empty())) {
        return linspace(RetTp{0}, RetTp{1}, 2);
    }

    const auto [first_edge, last_edge] = detail::outer_edges(x);
    const auto width = detail::bin_width<method>(x);

    if (units::fpclassify(width) == FP_ZERO) {
        return linspace(first_edge, last_edge, 2);
    }

    const auto n_equal_bins =
        std::size_t(units::value(ceil((last_edge - first_edge) / width)));
    return linspace(first_edge, last_edge, n_equal_bins + 1);
}

template <typename Array>
auto histogram_bin_edges(const Array &x, std::size_t nbins = 10) {
    const auto [first_edge, last_edge] = detail::outer_edges(x);
    return linspace(first_edge, last_edge, nbins + 1);
}

//---------------------------------------------------------------------------------
// histogram
//---------------------------------------------------------------------------------

constexpr bool UniformBins = true;
constexpr bool NonUniformBins = false;

constexpr bool Density = true;
constexpr bool Count = false;

template <
    bool density = false,
    bool use_uniform_bins = false,
    class InputIt,
    typename ItTp = typename std::iterator_traits<InputIt>::value_type,
    typename T = std::conditional_t<std::is_integral_v<ItTp>, double, ItTp>>
auto histogram(InputIt first, InputIt last, const std::vector<T> &bins) {
    using namespace operators;
    using raw_t = typename units::representation_t<T>;

    if (unlikely(bins.size() <= 1)) {
        if constexpr (density) {
            if constexpr (units::is_quantity_v<T>) {
                return empty<units::quantity_invert<T>>();
            } else {
                return empty<T>();
            }
        } else {
            return empty<signed_size_t>();
        }
    }

    scicpp_require(std::is_sorted(bins.cbegin(), bins.cend()));

    auto hist = zeros<signed_size_t>(bins.size() - 1);

    if constexpr (use_uniform_bins) {
        // No search required if uniformly distributed bins,
        // we can directly compute the index.

        const auto step = bins[1] - bins[0];
        scicpp_require(step > T{0});

        for (; first != last; ++first) {
            const auto pos = units::value((T(*first) - bins.front()) / step);

            if (pos >= raw_t(hist.size())) {
                if (almost_equal(T(*first), bins.back())) {
                    // Last bin edge is included
                    ++hist.back();
                }

                continue;
            }

            if (pos >= raw_t{0}) {
                ++hist[std::size_t(pos)];
            }
        }
    } else {
        // This works for both uniform and non-uniform bins.

        for (; first != last; ++first) {
            const auto it =
                std::upper_bound(bins.cbegin(), bins.cend(), *first);

            if (it == bins.cend()) { // No bin found
                if (almost_equal(T(*first), bins.back())) {
                    // Last bin edge is included
                    ++hist.back();
                }

                continue;
            }

            const auto pos = std::distance(bins.cbegin(), it) - 1;

            if (static_cast<raw_t>(pos) >= raw_t{0}) {
                ++hist[std::size_t(pos)];
            }
        }
    }

    // We only return the histogram for this overload,
    // the bins being an input argument.

    if constexpr (density) {
        return std::move(hist) / (static_cast<int>(sum(hist)) * diff(bins));
    } else {
        return hist;
    }
}

template <
    bool density = false,
    bool use_uniform_bins = false,
    class Array,
    typename ItTp = typename Array::value_type,
    typename T = std::conditional_t<std::is_integral_v<ItTp>, double, ItTp>>
auto histogram(const Array &x, const std::vector<T> &bins) {
    return histogram<density, use_uniform_bins>(x.cbegin(), x.cend(), bins);
}

template <BinEdgesMethod method, bool density = false, class Array>
auto histogram(const Array &x) {
    const auto bins = histogram_bin_edges<method>(x);
    return std::make_pair(histogram<density, UniformBins>(x, bins), bins);
}

template <bool density = false, class Array>
auto histogram(const Array &x, std::size_t nbins = 10) {
    const auto bins = histogram_bin_edges(x, nbins);
    return std::make_pair(histogram<density, UniformBins>(x, bins), bins);
}

} // namespace scicpp::stats

#endif // SCICPP_CORE_HISTOGRAM