// SPDX-License-Identifier: MIT
// Copyright (c) 2019-2021 Thomas Vanderbruggen <th.vanderbruggen@gmail.com>

#ifndef SCICPP_CORE_EQUAL
#define SCICPP_CORE_EQUAL

#include "scicpp/core/macros.hpp"
#include "scicpp/core/meta.hpp"
#include "scicpp/core/units/quantity.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string_view>

namespace scicpp {

//---------------------------------------------------------------------------------
// almost_equal
//---------------------------------------------------------------------------------

namespace detail {

template <typename T>
bool is_zero(T a) {
    return std::fpclassify(a) == FP_ZERO;
}

template <int rel_tol, typename T>
bool fp_equal_predicate(T a, T b) {
    static_assert(rel_tol >= 0);

    constexpr auto eps = std::numeric_limits<T>::epsilon();
    constexpr auto tol = T(rel_tol) * eps / T{2};

    if (std::isnan(a) && std::isnan(b)) {
        return true;
    }

    if (std::isinf(a) && std::isinf(b)) {
        return std::signbit(a) == std::signbit(b);
    }

    const auto max_val = std::max(std::fabs(a), std::fabs(b));

    if (is_zero(a) || is_zero(b) || max_val < tol) {
        return std::fabs(a - b) <= tol;
    }

    return std::fabs(a - b) <= tol * max_val;
}

} // namespace detail

template <int rel_tol = 1,
          typename T,
          meta::disable_if_iterable<T> = 0,
          units::disable_if_is_quantity<T> = 0>
bool almost_equal(T a, T b) {
    if constexpr (meta::is_complex_v<T>) {
        using scal_t = typename T::value_type;

        if constexpr (units::is_quantity_v<scal_t>) {
            return almost_equal<rel_tol>(a.real().eval(), b.real().eval()) &&
                   almost_equal<rel_tol>(a.imag().eval(), b.imag().eval());
        } else {
            return (detail::fp_equal_predicate<rel_tol>(a.real(), b.real())) &&
                   (detail::fp_equal_predicate<rel_tol>(a.imag(), b.imag()));
        }
    } else {
        return detail::fp_equal_predicate<rel_tol>(a, b);
    }
}

template <int rel_tol = 1,
          typename T,
          typename Dim,
          typename Scale1,
          typename Scale2,
          typename Offset1,
          typename Offset2>
auto almost_equal(const units::quantity<T, Dim, Scale1, Offset1> &q1,
                  const units::quantity<T, Dim, Scale2, Offset2> &q2) {
    return almost_equal<rel_tol>(q1.eval(), q2.eval());
}

template <int rel_tol = 1, class Array, meta::enable_if_iterable<Array> = 0>
bool scicpp_pure almost_equal(const Array &f1, const Array &f2) {
    return std::equal(
        f1.cbegin(), f1.cend(), f2.cbegin(), f2.cend(), [](auto a, auto b) {
            return almost_equal<rel_tol>(a, b);
        });
}

template <class Array, meta::enable_if_iterable<Array> = 0>
bool scicpp_pure array_equal(const Array &f1, const Array &f2) {
    return std::equal(f1.cbegin(), f1.cend(), f2.cbegin(), f2.cend());
}

//---------------------------------------------------------------------------------
// strings_equal
// https://stackoverflow.com/questions/27490858/how-can-you-compare-two-character-strings-statically-at-compile-time
//---------------------------------------------------------------------------------

constexpr bool strings_equal(char const *a, char const *b) {
    return std::string_view(a) == b;
}

} // namespace scicpp

#endif // SCICPP_CORE_EQUAL