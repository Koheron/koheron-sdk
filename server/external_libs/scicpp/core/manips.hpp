// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Thomas Vanderbruggen <th.vanderbruggen@gmail.com>

// Array manipulation

#ifndef SCICPP_CORE_MANIPS
#define SCICPP_CORE_MANIPS

#include "scicpp/core/macros.hpp"
#include "scicpp/core/meta.hpp"
#include "scicpp/core/range.hpp"
#include "scicpp/core/units/quantity.hpp"
#include "scicpp/core/utils.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <iterator>
#include <ranges>
#include <type_traits>
#include <vector>

namespace scicpp {

//-----------------------------------------------------------------------------
// concatenate
//-----------------------------------------------------------------------------

template <typename T1, std::size_t N1, typename T2, std::size_t N2>
constexpr auto concatenate(const std::array<T1, N1> &a1,
                           const std::array<T2, N2> &a2) {
    if constexpr (units::is_quantity_v<T1>) {
        static_assert(units::is_same_dimension<T1, T2>);

        std::array<T1, N1 + N2> res{};
        std::copy(a1.cbegin(), a1.cend(), res.begin());
        std::transform(a2.cbegin(),
                       a2.cend(),
                       res.begin() + signed_size_t(a1.size()),
                       [](auto x) { return units::quantity_cast<T1>(x); });
        return res;
    } else {
        std::array<std::common_type_t<T1, T2>, N1 + N2> res{};
        std::copy(a1.cbegin(), a1.cend(), res.begin());
        std::copy(
            a2.cbegin(), a2.cend(), res.begin() + signed_size_t(a1.size()));
        return res;
    }
}

template <meta::Iterable Array1, meta::Iterable Array2>
auto concatenate(const Array1 &a1, const Array2 &a2) {
    using T1 = Array1::value_type;
    using T2 = Array2::value_type;

    if constexpr (units::is_quantity_v<T1>) {
        static_assert(units::is_same_dimension<T1, T2>);

        std::vector<T1> res(a1.cbegin(), a1.cend());
        res.resize(a1.size() + a2.size());
        std::transform(a2.cbegin(),
                       a2.cend(),
                       res.begin() + signed_size_t(a1.size()),
                       [](auto x) { return units::quantity_cast<T1>(x); });
        return res;
    } else {
        std::vector<std::common_type_t<T1, T2>> res(a1.cbegin(), a1.cend());
        res.resize(a1.size() + a2.size());
        std::copy(
            a2.cbegin(), a2.cend(), res.begin() + signed_size_t(a1.size()));
        return res;
    }
}

template <meta::Iterable Array, typename T>
auto concatenate(std::vector<T> &&a1, const Array &a2) {
    using Tarray = Array::value_type;

    const auto N1 = a1.size();
    a1.resize(N1 + a2.size());

    if constexpr (units::is_quantity_v<T>) {
        static_assert(units::is_same_dimension<T, Tarray>);

        std::transform(a2.cbegin(),
                       a2.cend(),
                       a1.begin() + signed_size_t(N1),
                       [](auto x) { return units::quantity_cast<T>(x); });
    } else if constexpr (std::is_same_v<T, Tarray>) {
        std::copy(a2.cbegin(), a2.cend(), a1.begin() + signed_size_t(N1));
    } else if constexpr (std::is_convertible_v<T, Tarray>) {
        std::transform(a2.cbegin(),
                       a2.cend(),
                       a1.begin() + signed_size_t(N1),
                       [](auto x) { return static_cast<T>(x); });
    }

    return std::move(a1);
}

template <meta::Iterable Array,
          typename T,
          std::enable_if_t<std::is_lvalue_reference_v<Array>, int> = 0>
auto concatenate(const Array &a1, std::vector<T> &&a2) {
    using Tarray = Array::value_type;

    a2.reserve(a1.size() + a2.size());

    if constexpr (units::is_quantity_v<T>) {
        static_assert(units::is_same_dimension<T, Tarray>);

        std::transform(a1.cbegin(),
                       a1.cend(),
                       std::inserter(a2, a2.begin()),
                       [](auto x) { return units::quantity_cast<T>(x); });
    } else if constexpr (std::is_same_v<T, Tarray>) {
        a2.insert(a2.begin(), a1.cbegin(), a1.cend());
    } else if constexpr (std::is_convertible_v<T, Tarray>) {
        std::transform(a1.cbegin(),
                       a1.cend(),
                       std::inserter(a2, a2.begin()),
                       [](auto x) { return static_cast<T>(x); });
    }

    return std::move(a2);
}

namespace operators {

// Define a concatenation operator |

template <meta::Iterable ArrayLhs, meta::Iterable ArrayRhs>
constexpr auto operator|(ArrayLhs &&a, ArrayRhs &&b) {
    return concatenate(std::forward<ArrayLhs>(a), std::forward<ArrayRhs>(b));
}

} // namespace operators

template <meta::Iterable... Arrays>
constexpr auto concatenate(Arrays &&...a) {
    using namespace operators;
    return (std::forward<Arrays>(a) | ...);
}

//-----------------------------------------------------------------------------
// flip
//-----------------------------------------------------------------------------

template <typename Array>
constexpr auto flip_inplace(Array &a) {
    std::reverse(a.begin(), a.end());
}

template <typename Array>
constexpr auto flip(Array &&a) {
    flip_inplace(a);
    return std::forward<Array>(a);
}

template <typename Array>
constexpr auto flip(const Array &a) {
    auto res = utils::set_array(a);
    std::reverse_copy(a.cbegin(), a.cend(), res.begin());
    return res;
}

//-----------------------------------------------------------------------------
// slice_array
//
// return the result of a[slice(start, stop, step)]
//-----------------------------------------------------------------------------

template <std::ranges::random_access_range R>
scicpp_pure auto slice_array(const R &r,
                             std::ptrdiff_t start,
                             std::ptrdiff_t stop,
                             std::ptrdiff_t step = 1) {
    using T = std::ranges::range_value_t<R>;
    std::vector<T> out;
    scicpp_require(step != 0);

    const std::ptrdiff_t n = std::ranges::ssize(r);
    if (n == 0) {
        return out;
    }

    // normalize negatives: [-n, n-1] => [0, n] for bounds handling
    auto norm_neg = [n](std::ptrdiff_t i) { return (i < 0) ? (i + n) : i; };

    if (step > 0) {
        // clamp to [0, n] (note: stop is exclusive)
        start = std::clamp(norm_neg(start), std::ptrdiff_t{0}, n);
        stop = std::clamp(norm_neg(stop), std::ptrdiff_t{0}, n);

        if (start >= stop) {
            return out;
        }

        const auto span = stop - start;
        const auto count = (span + (step - 1)) / step;
        out.reserve(static_cast<std::size_t>(count));

        auto it0 = std::ranges::begin(r);

        for (auto i = start; i < stop; i += step) {
            out.push_back(*(it0 + i));
        }
    } else { // step < 0
        // for negative step, valid element indices are [0, n-1]
        // stop is exclusive; typical Python semantics iterate while i > stop
        start = std::clamp(norm_neg(start), std::ptrdiff_t{0}, n - 1);
        stop = std::clamp(norm_neg(stop), std::ptrdiff_t{-1}, n - 1);

        if (start <= stop) {
            return out;
        }

        const auto span = start - stop;
        const auto k = -step;
        const auto count = (span + (k - 1)) / k;
        out.reserve(static_cast<std::size_t>(count));

        auto it0 = std::ranges::begin(r);

        for (auto i = start; i > stop; i += step) { // step is negative
            out.push_back(*(it0 + i));
        }
    }

    return out;
}

} // namespace scicpp

#endif // SCICPP_CORE_MANIPS