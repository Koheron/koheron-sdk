// SPDX-License-Identifier: MIT
// Copyright (c) 2019-2021 Thomas Vanderbruggen <th.vanderbruggen@gmail.com>

#ifndef SCICPP_CORE_STATS
#define SCICPP_CORE_STATS

#include "scicpp/core/equal.hpp"
#include "scicpp/core/functional.hpp"
#include "scicpp/core/macros.hpp"
#include "scicpp/core/maths.hpp"
#include "scicpp/core/numeric.hpp"
#include "scicpp/core/units/quantity.hpp"

#include <Eigen/Dense>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <limits>
#include <tuple>
#include <type_traits>
#include <vector>

namespace scicpp::stats {

namespace detail {

template <class Array>
auto quiet_nan() {
    return std::numeric_limits<typename Array::value_type>::quiet_NaN();
}

} // namespace detail

//---------------------------------------------------------------------------------
// amax
//---------------------------------------------------------------------------------

template <class Array>
constexpr scicpp_pure auto amax(const Array &f) {
    if (unlikely(f.empty())) {
        return detail::quiet_nan<Array>();
    }

    return *std::max_element(f.cbegin(), f.cend());
}

//---------------------------------------------------------------------------------
// amin
//---------------------------------------------------------------------------------

template <class Array>
constexpr scicpp_pure auto amin(const Array &f) {
    if (unlikely(f.empty())) {
        return detail::quiet_nan<Array>();
    }

    return *std::min_element(f.cbegin(), f.cend());
}

//---------------------------------------------------------------------------------
// ptp
//---------------------------------------------------------------------------------

template <class Array>
constexpr scicpp_pure auto ptp(const Array &f) {
    if (unlikely(f.empty())) {
        return detail::quiet_nan<Array>();
    }

    const auto [it_min, it_max] = std::minmax_element(f.cbegin(), f.cend());
    return *it_max - *it_min;
}

//---------------------------------------------------------------------------------
// average
//---------------------------------------------------------------------------------

template <class Array1, class Array2>
constexpr auto average(const Array1 &f, const Array2 &weights) {
    if (unlikely(f.empty() || (f.size() != weights.size()))) {
        return detail::quiet_nan<Array1>();
    }

    return inner(f, weights) / sum(weights);
}

//---------------------------------------------------------------------------------
// median
//---------------------------------------------------------------------------------

namespace detail {

// https://stackoverflow.com/questions/1719070/what-is-the-right-approach-when-using-stl-container-for-median-calculation
template <class InputIt>
auto median_inplace(InputIt first, InputIt last) {
    using T = typename std::iterator_traits<InputIt>::value_type;
    using raw_t = units::representation_t<T>;
    const auto size = std::distance(first, last);

    if (unlikely(size == 0)) {
        return std::numeric_limits<T>::quiet_NaN();
    }

    const signed_size_t half = size / 2;
    const auto target = first + half;
    std::nth_element(first, target, last);

    if (size % 2 != 0) { // vector size is odd
        return *target;
    } else {
        const auto max_it = std::max_element(first, first + half);
        return (*max_it + *target) / raw_t{2}; // cf. std::midpoint (C++20)
    }
}

} // namespace detail

template <class InputIt, class Predicate>
auto median(InputIt first, InputIt last, Predicate p) {
    auto v = filter(std::vector(first, last), p);
    return detail::median_inplace(v.begin(), v.end());
}

template <class Array, class Predicate>
auto median(const Array &f, Predicate filter) {
    return median(f.cbegin(), f.cend(), filter);
}

template <class Array>
auto median(Array &&f) {
    if constexpr (std::is_lvalue_reference_v<Array>) {
        auto tmp = f;
        return detail::median_inplace(tmp.begin(), tmp.end());
    } else {
        return detail::median_inplace(f.begin(), f.end());
    }
}

template <class Array>
auto nanmedian(const Array &f) {
    return median(f, filters::not_nan);
}

//---------------------------------------------------------------------------------
// quantile, percentile, iqr
//---------------------------------------------------------------------------------

enum class QuantileInterp : int { LOWER, HIGHER, NEAREST, MIDPOINT, LINEAR };

namespace detail {

template <QuantileInterp interpolation, typename T>
auto quantile_interp_index(T h) {
    if constexpr (interpolation == QuantileInterp::LOWER) {
        return std::floor(h);
    } else if constexpr (interpolation == QuantileInterp::HIGHER) {
        return std::ceil(h);
    } else if constexpr (interpolation == QuantileInterp::NEAREST) {
        return std::nearbyint(h);
    } else if constexpr (interpolation == QuantileInterp::MIDPOINT) {
        // cf. std::midpoint (C++20)
        return T{0.5} * (std::floor(h) + std::ceil(h));
    } else { // interpolation == LINEAR
        return h;
    }
}

// https://stackoverflow.com/questions/28548703/why-does-stdnth-element-return-sorted-vectors-for-input-vectors-with-n-33-el
template <QuantileInterp interpolation, class InputIt, typename T>
auto quantile_inplace(InputIt first, InputIt last, T q) {
    scicpp_require(q >= T{0} && q <= T{1});

    using ItTp = typename std::iterator_traits<InputIt>::value_type;
    using RetTp = std::conditional_t<std::is_integral_v<ItTp>, double, ItTp>;

    const auto size = std::distance(first, last);

    if (unlikely(size == 0)) {
        return std::numeric_limits<RetTp>::quiet_NaN();
    }

    if (size == 1) {
        return RetTp(*first);
    }

    const auto h0 =
        quantile_interp_index<interpolation>(q * static_cast<T>(size - 1));

    if (almost_equal(std::nearbyint(h0), h0)) { // h0 is an integer
        const auto n0 = std::min(first + signed_size_t(h0), last);
        std::nth_element(first, n0, last);
        return RetTp(*n0);
    } else { // h0 not an integral index
        const auto h_low = signed_size_t(h0);
        const auto n_high = std::min(first + h_low + 1, last);
        std::nth_element(first, n_high, last);
        const auto x_low = *std::max_element(first, n_high);
        const auto x_high = *n_high;
        return lerp(x_low, x_high, h0 - std::floor(h_low));
    }
}

} // namespace detail

template <QuantileInterp interpolation = QuantileInterp::LINEAR,
          class InputIt,
          class Predicate,
          typename T>
auto quantile(InputIt first, InputIt last, T q, Predicate p) {
    auto v = filter(std::vector(first, last), p);
    return detail::quantile_inplace<interpolation>(v.begin(), v.end(), q);
}

template <QuantileInterp interpolation = QuantileInterp::LINEAR,
          class Array,
          class Predicate,
          typename T>
auto quantile(const Array &f, T q, Predicate filter) {
    return quantile<interpolation>(f.cbegin(), f.cend(), q, filter);
}

template <QuantileInterp interpolation = QuantileInterp::LINEAR,
          class Array,
          typename T>
auto quantile(Array &&f, T q) {
    if constexpr (std::is_lvalue_reference_v<Array>) {
        auto tmp = f;
        return detail::quantile_inplace<interpolation>(
            tmp.begin(), tmp.end(), q);
    } else {
        return detail::quantile_inplace<interpolation>(f.begin(), f.end(), q);
    }
}

template <QuantileInterp interpolation = QuantileInterp::LINEAR,
          class Array,
          typename T>
auto nanquantile(const Array &f, T q) {
    return quantile<interpolation>(f, q, filters::not_nan);
}

template <QuantileInterp interpolation = QuantileInterp::LINEAR,
          class InputIt,
          class Predicate,
          typename T>
auto percentile(InputIt first, InputIt last, T p, Predicate filter) {
    return quantile<interpolation>(first, last, p / 100., filter);
}

template <QuantileInterp interpolation = QuantileInterp::LINEAR,
          class Array,
          class Predicate,
          typename T>
auto percentile(const Array &f, T p, Predicate filter) {
    return quantile<interpolation>(f, p / 100., filter);
}

template <QuantileInterp interpolation = QuantileInterp::LINEAR,
          class Array,
          typename T>
auto percentile(Array &&f, T p) {
    return quantile<interpolation>(std::forward<Array>(f), p / 100.);
}

template <QuantileInterp interpolation = QuantileInterp::LINEAR,
          class Array,
          typename T>
auto nanpercentile(const Array &f, T p) {
    return nanquantile<interpolation>(f, p / 100.);
}

template <QuantileInterp interpolation = QuantileInterp::LINEAR, class Array>
auto iqr(Array &&f, double rng0 = 25., double rng1 = 75.) {
    const auto pct0 = percentile<interpolation>(std::forward<Array>(f), rng0);
    const auto pct1 = percentile<interpolation>(std::forward<Array>(f), rng1);
    return pct1 - pct0;
}

template <QuantileInterp interpolation = QuantileInterp::LINEAR, class Array>
auto iqr(const Array &f, double rng0 = 25., double rng1 = 75.) {
    const auto pct0 = percentile<interpolation>(f, rng0);
    const auto pct1 = percentile<interpolation>(f, rng1);
    return pct1 - pct0;
}

template <QuantileInterp interpolation = QuantileInterp::LINEAR, class Array>
auto naniqr(Array &&f, double rng0 = 25., double rng1 = 75.) {
    const auto pct0 =
        nanpercentile<interpolation>(std::forward<Array>(f), rng0);
    const auto pct1 =
        nanpercentile<interpolation>(std::forward<Array>(f), rng1);
    return pct1 - pct0;
}

template <QuantileInterp interpolation = QuantileInterp::LINEAR, class Array>
auto naniqr(const Array &f, double rng0 = 25., double rng1 = 75.) {
    const auto pct0 = nanpercentile<interpolation>(f, rng0);
    const auto pct1 = nanpercentile<interpolation>(f, rng1);
    return pct1 - pct0;
}

//---------------------------------------------------------------------------------
// mean
//---------------------------------------------------------------------------------

template <class InputIt, class Predicate>
constexpr auto mean(InputIt first, InputIt last, Predicate filter) {
    using T = typename std::iterator_traits<InputIt>::value_type;

    if (unlikely(std::distance(first, last) == 0)) {
        return std::numeric_limits<T>::quiet_NaN();
    }

    const auto [res, cnt] = sum(first, last, filter);
    return res / units::representation_t<T>(static_cast<int>(cnt));
}

template <class Array, class Predicate>
constexpr auto mean(const Array &f, Predicate filter) {
    return mean(f.cbegin(), f.cend(), filter);
}

template <class Array>
constexpr auto mean(const Array &f) {
    return mean(f, filters::all);
}

template <class Array>
auto nanmean(const Array &f) {
    return mean(f, filters::not_nan);
}

template <class Array, typename T = typename Array::value_type>
constexpr auto tmean(const Array &f,
                     const std::array<T, 2> &limits,
                     const std::array<bool, 2> &inclusive = {true, true}) {
    return mean(f, filters::Trim<T>(limits, inclusive));
}

//---------------------------------------------------------------------------------
// gmean
//---------------------------------------------------------------------------------

template <class Array>
auto gmean(Array &&f) {
    using T = typename std::decay_t<Array>::value_type;

    if (unlikely(f.empty())) {
        return std::numeric_limits<T>::quiet_NaN();
    }

    if constexpr (units::is_quantity_v<T>) {
        using namespace operators;
        return T(std::exp(mean(log(std::forward<Array>(f) / T(1)))));
    } else {
        return std::exp(mean(log(std::forward<Array>(f))));
    }
}

template <class Array, class Predicate>
auto gmean(Array &&f, Predicate p) {
    return gmean(filter(std::forward<Array>(f), p));
}

template <class Array>
auto nangmean(Array &&f) {
    return gmean(std::forward<Array>(f), filters::not_nan);
}

//---------------------------------------------------------------------------------
// covariance
//---------------------------------------------------------------------------------

template <int ddof = 0, class InputIt1, class InputIt2, class Predicate>
constexpr auto covariance(InputIt1 first1,
                          InputIt1 last1,
                          InputIt2 first2,
                          InputIt2 last2,
                          Predicate filter) {
    using T1 = typename std::iterator_traits<InputIt1>::value_type;
    using T2 = typename std::iterator_traits<InputIt2>::value_type;
    using raw_t1 = units::representation_t<T1>;
    using raw_t2 = units::representation_t<T2>;
    using raw_t = std::common_type_t<raw_t1, raw_t2>;
    using prod_t = decltype(std::declval<T1>() * std::declval<T2>());

    static_assert(meta::is_predicate<Predicate, T1>);
    static_assert(meta::is_predicate<Predicate, T2>);

    scicpp_require(std::distance(first1, last1) ==
                   std::distance(first2, last2));

    if (unlikely(std::distance(first1, last1) == 0)) {
        return std::tuple{std::numeric_limits<prod_t>::quiet_NaN(),
                          signed_size_t(0)};
    }

    // Pairwise recursive implementation of covariance summation
    const auto [m1_, m2_, cov_, c_] = pairwise_accumulate<64>(
        first1,
        last1,
        first2,
        last2,
        [&](auto f1, auto l1, auto f2, auto l2) {
            const auto m1 = mean(f1, l1, filter);
            const auto m2 = mean(f2, l2, filter);

            auto res = utils::set_zero<prod_t>();
            signed_size_t cnt = 0;

            for (; f1 != l1; ++f1, (void)++f2) {
                if (filter(*f1) && filter(*f2)) {
                    if constexpr (meta::is_complex_v<T2>) {
                        res += (*f1 - m1) * std::conj(*f2 - m2);
                    } else {
                        res += (*f1 - m1) * (*f2 - m2);
                    }
                    cnt++;
                }
            }

            return std::tuple{m1, m2, res, cnt};
        },
        [&](const auto res1, const auto res2) {
            // Combine covariances
            // https://stackoverflow.com/questions/45773857/merging-covariance-from-two-sets-to-create-new-covariance
            const auto [m11, m12, covar1, n1] = res1;
            const auto [m21, m22, covar2, n2] = res2;

            const auto n_c = n1 + n2;
            const auto m1_c = (raw_t1{1} / raw_t1(static_cast<int>(n_c))) *
                              (raw_t1(static_cast<int>(n1)) * m11 +
                               raw_t1(static_cast<int>(n2)) * m21);
            const auto m2_c = (raw_t2{1} / raw_t2(static_cast<int>(n_c))) *
                              (raw_t2(static_cast<int>(n1)) * m12 +
                               raw_t2(static_cast<int>(n2)) * m22);
            const auto covar_c =
                covar1 + covar2 +
                (raw_t(static_cast<int>(n1)) * raw_t(static_cast<int>(n2)) /
                 raw_t(static_cast<int>(n_c))) *
                    conj(m12 - m22) * (m11 - m21);
            return std::tuple{m1_c, m2_c, covar_c, n_c};
        });

    if (unlikely(c_ - ddof <= 0)) {
        return std::tuple{std::numeric_limits<decltype(cov_)>::infinity(), c_};
    } else {
        return std::tuple{cov_ / raw_t(static_cast<int>(c_) - ddof), c_};
    }
}

template <int ddof = 0, class Array1, class Array2, class Predicate>
constexpr auto
covariance(const Array1 &f1, const Array2 &f2, Predicate filter) {
    return std::get<0>(covariance<ddof>(
        f1.cbegin(), f1.cend(), f2.cbegin(), f2.cend(), filter));
}

template <int ddof = 0, class Array1, class Array2>
constexpr auto covariance(const Array1 &f1, const Array2 &f2) {
    return covariance<ddof>(f1, f2, filters::all);
}

template <int ddof = 0, class Array1, class Array2>
auto nancovariance(const Array1 &f1, const Array2 &f2) {
    return covariance<ddof>(f1, f2, filters::not_nan);
}

//---------------------------------------------------------------------------------
// var
//---------------------------------------------------------------------------------

template <int ddof = 0, class InputIt, class Predicate>
constexpr auto var(InputIt first, InputIt last, Predicate filter) {
    const auto [v, n] = covariance<ddof>(first, last, first, last, filter);
    using T = std::decay_t<decltype(v)>;

    if constexpr (meta::is_complex_v<T>) {
        // The variance is always a nonnegative real number
        return std::tuple{std::real(v), n};
    } else {
        return std::tuple{v, n};
    }
}

template <int ddof = 0, class Array, class Predicate>
constexpr auto var(const Array &f, Predicate filter) {
    return std::get<0>(var<ddof>(f.cbegin(), f.cend(), filter));
}

template <int ddof = 0, class Array>
constexpr auto var(const Array &f) {
    return var<ddof>(f, filters::all);
}

template <int ddof = 0, class Array>
auto nanvar(const Array &f) {
    return var<ddof>(f, filters::not_nan);
}

template <int ddof = 1, class Array, typename T = typename Array::value_type>
constexpr auto tvar(const Array &f,
                    const std::array<T, 2> &limits,
                    const std::array<bool, 2> &inclusive = {true, true}) {
    return var<ddof>(f, filters::Trim<T>(limits, inclusive));
}

//---------------------------------------------------------------------------------
// std
//---------------------------------------------------------------------------------

template <int ddof = 0, class Array, class Predicate>
auto std(const Array &a, Predicate filter) {
    return units::sqrt(var<ddof>(a, filter));
}

template <int ddof = 0, class Array>
auto std(const Array &a) {
    return units::sqrt(var<ddof>(a));
}

template <int ddof = 0, class Array>
auto nanstd(const Array &a) {
    return units::sqrt(nanvar<ddof>(a));
}

template <int ddof = 1, class Array, typename T = typename Array::value_type>
auto tstd(const Array &a,
          const std::array<T, 2> &limits,
          const std::array<bool, 2> &inclusive = {true, true}) {
    return units::sqrt(tvar<ddof>(a, limits, inclusive));
}

//---------------------------------------------------------------------------------
// sem
//---------------------------------------------------------------------------------

template <int ddof = 1, class Array, class Predicate>
auto sem(const Array &a, Predicate filter) {
    const auto [v, n] = var<ddof>(a.cbegin(), a.cend(), filter);
    using T = std::decay_t<decltype(v)>;
    using raw_t = units::representation_t<T>;
    return units::sqrt(v / raw_t(n));
}

template <int ddof = 1, class Array>
auto sem(const Array &a) {
    const auto v = var<ddof>(a);
    using T = std::decay_t<decltype(v)>;
    using raw_t = units::representation_t<T>;
    return units::sqrt(v / raw_t(a.size()));
}

template <int ddof = 1, class Array>
auto nansem(const Array &a) {
    return sem<ddof>(a, filters::not_nan);
}

template <int ddof = 1, class Array, typename T = typename Array::value_type>
constexpr auto tsem(const Array &f,
                    const std::array<T, 2> &limits,
                    const std::array<bool, 2> &inclusive = {true, true}) {
    return sem<ddof>(f, filters::Trim<T>(limits, inclusive));
}

//---------------------------------------------------------------------------------
// moment
//---------------------------------------------------------------------------------

template <intmax_t n, class Array, class Predicate>
auto moment(const Array &f, [[maybe_unused]] Predicate filter) {
    using namespace operators;
    using T = typename Array::value_type;

    if constexpr (n == 0) {
        return T{1};
    } else if constexpr (n == 1) {
        return T{0};
    } else if constexpr (n == 2) {
        return var(f, filter);
    } else {
        // This allocates an extra array,
        // but preserves pairwise recursion precision
        return mean(pow<n>(f - mean(f, filter)), filter);
        // Combination of moments
        // http://prod.sandia.gov/techlib/access-control.cgi/2008/086212.pdf
    }
}

template <intmax_t n, class Array>
auto moment(const Array &f) {
    return moment<n>(f, filters::all);
}

template <intmax_t n, class Array>
auto nanmoment(const Array &f) {
    return moment<n>(f, filters::not_nan);
}

//---------------------------------------------------------------------------------
// kurtosis
//---------------------------------------------------------------------------------

enum KurtosisDef { Fisher = 0, Pearson = 1 };

template <KurtosisDef def = KurtosisDef::Fisher, class Array, class Predicate>
auto kurtosis(const Array &f, Predicate filter) {
    const auto m2 = moment<2>(f, filter);
    const auto m4 = moment<4>(f, filter);
    const auto k = m4 / (m2 * m2);

    if constexpr (def == KurtosisDef::Fisher) {
        using T = decltype(k);
        return k - T(3);
    } else {
        return k;
    }
}

template <KurtosisDef def = KurtosisDef::Fisher, class Array>
auto kurtosis(const Array &f) {
    return kurtosis<def>(f, filters::all);
}

template <KurtosisDef def = KurtosisDef::Fisher, class Array>
auto nankurtosis(const Array &f) {
    return kurtosis<def>(f, filters::not_nan);
}

//---------------------------------------------------------------------------------
// skew
//---------------------------------------------------------------------------------

template <class Array, class Predicate>
auto skew(const Array &f, Predicate filter) {
    const auto m2 = moment<2>(f, filter);
    const auto m3 = moment<3>(f, filter);
    return m3 / units::sqrt(m2 * m2 * m2);
}

template <class Array>
auto skew(const Array &f) {
    return skew(f, filters::all);
}

template <class Array>
auto nanskew(const Array &f) {
    return skew(f, filters::not_nan);
}

//---------------------------------------------------------------------------------
// covariance matrix
//---------------------------------------------------------------------------------

template <int ddof = 1, class Array1, class Array2, class Predicate>
auto cov(const Array1 &f1, const Array2 &f2, Predicate filter) {
    const auto covar = covariance<ddof>(f1, f2, filter);
    using T = std::decay_t<decltype(covar)>;

    Eigen::Matrix<T, 2, 2> res;
    res(0, 0) = T(var<ddof>(f1, filter));
    res(0, 1) = covar;
    res(1, 0) = conj(covar);
    res(1, 1) = T(var<ddof>(f2, filter));
    return res;
}

template <int ddof = 1, class Array1, class Array2>
auto cov(const Array1 &f1, const Array2 &f2) {
    return cov<ddof>(f1, f2, filters::all);
}

template <int ddof = 1, class Array1, class Array2>
auto nancov(const Array1 &f1, const Array2 &f2) {
    return cov<ddof>(f1, f2, filters::not_nan);
}

} // namespace scicpp::stats

#endif // SCICPP_CORE_STATS
