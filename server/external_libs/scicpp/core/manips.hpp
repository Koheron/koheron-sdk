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
#include <iterator>
#include <type_traits>
#include <vector>

namespace scicpp {

//-----------------------------------------------------------------------------
// concatenate
//-----------------------------------------------------------------------------

template <typename T1, std::size_t N1, typename T2, std::size_t N2>
auto concatenate(const std::array<T1, N1> &a1, const std::array<T2, N2> &a2) {
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

template <typename Array1,
          typename Array2,
          meta::enable_if_iterable<Array1> = 0,
          meta::enable_if_iterable<Array2> = 0>
auto concatenate(const Array1 &a1, const Array2 &a2) {
    using T1 = typename Array1::value_type;
    using T2 = typename Array2::value_type;

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

template <typename Array, typename T, meta::enable_if_iterable<Array> = 0>
auto concatenate(std::vector<T> &&a1, const Array &a2) {
    using Tarray = typename Array::value_type;

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

template <typename Array, typename T, meta::enable_if_iterable<Array> = 0>
auto concatenate(const Array &a1, std::vector<T> &&a2) {
    using Tarray = typename Array::value_type;

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

template <class ArrayLhs,
          class ArrayRhs,
          meta::enable_if_iterable<ArrayLhs> = 0,
          meta::enable_if_iterable<ArrayRhs> = 0>
auto operator|(ArrayLhs &&a, ArrayRhs &&b) {
    return concatenate(std::forward<ArrayLhs>(a), std::forward<ArrayRhs>(b));
}

} // namespace operators

template <typename... Arrays>
auto concatenate(Arrays &&...a) {
    static_assert((meta::is_iterable_v<Arrays> && ...));
    using namespace operators;
    return (std::forward<Arrays>(a) | ...);
}

//-----------------------------------------------------------------------------
// flip
//-----------------------------------------------------------------------------

template <typename Array>
auto flip_inplace(Array &a) {
    std::reverse(a.begin(), a.end());
}

template <typename Array>
auto flip(Array &&a) {
    flip_inplace(a);
    return std::forward<Array>(a);
}

template <typename Array>
auto flip(const Array &a) {
    auto res = utils::set_array(a);
    std::reverse_copy(a.cbegin(), a.cend(), res.begin());
    return res;
}

} // namespace scicpp

#endif // SCICPP_CORE_MANIPS