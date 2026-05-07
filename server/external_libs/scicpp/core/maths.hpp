// SPDX-License-Identifier: MIT
// Copyright (c) 2019-2021 Thomas Vanderbruggen <th.vanderbruggen@gmail.com>

#ifndef SCICPP_CORE_MATHS
#define SCICPP_CORE_MATHS

#include "scicpp/core/equal.hpp"
#include "scicpp/core/functional.hpp"
#include "scicpp/core/macros.hpp"
#include "scicpp/core/meta.hpp"
#include "scicpp/core/units/units.hpp"

#include <array>
#include <cmath>
#include <complex>
#include <cstdlib>
#include <limits>
#include <numeric>
#include <type_traits>
#include <vector>

namespace scicpp {

//---------------------------------------------------------------------------------
// fabs
//
// std::fabs is not constexpr
//---------------------------------------------------------------------------------

template <typename T>
constexpr auto fabs(T &&x) {
    if constexpr (meta::Iterable<T>) {
        return map([](auto v) { return fabs(v); }, std::forward<T>(x));
    } else {
        using U = std::decay_t<T>;
        // Handles negative zero
        // https://codereview.stackexchange.com/questions/60140/generic-absolute-value-function
        // Could use std::fpclassify(x) == FP_ZERO
        // to quiet warning, but this is not constexpr.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
        if (units::value(x) == units::representation_t<U>(0.)) {
            return U(+0.);
        }
#pragma GCC diagnostic pop

        return (x < U(0.)) ? -x : x;
    }
}

//---------------------------------------------------------------------------------
// vectorized maths functions
//---------------------------------------------------------------------------------

// Trigonometric functions

const auto sin = vectorize([](auto x) { return units::sin(x); });
const auto cos = vectorize([](auto x) { return units::cos(x); });
const auto tan = vectorize([](auto x) { return units::tan(x); });

const auto arcsin = vectorize([](auto x) { return units::asin(x); });
const auto arccos = vectorize([](auto x) { return units::acos(x); });
const auto arctan = vectorize([](auto x) { return units::atan(x); });
const auto arctan2 =
    vectorize([](auto x, auto y) { return units::atan2(x, y); });
const auto hypot = vectorize([](auto x, auto y) { return units::hypot(x, y); });

const auto sinc = vectorize([](auto x) { return units::sinc(x); });

// Hyperbolic functions

const auto sinh = vectorize([](auto x) { return std::sinh(x); });
const auto cosh = vectorize([](auto x) { return std::cosh(x); });
const auto tanh = vectorize([](auto x) { return std::tanh(x); });
const auto arcsinh = vectorize([](auto x) { return std::asinh(x); });
const auto arccosh = vectorize([](auto x) { return std::acosh(x); });
const auto arctanh = vectorize([](auto x) { return std::atanh(x); });

// Exponents and logarithms

const auto exp = vectorize([](auto x) { return units::exp(x); });
const auto expm1 = vectorize([](auto x) { return units::expm1(x); });
const auto exp2 = vectorize([](auto x) { return units::exp2(x); });
const auto log = vectorize([](auto x) { return units::log(x); });
const auto log2 = vectorize([](auto x) { return units::log2(x); });
const auto log10 = vectorize([](auto x) { return units::log10(x); });
const auto log1p = vectorize([](auto x) { return units::log1p(x); });

// Rounding

const auto around = vectorize([](auto x) { return units::round(x); });
const auto floor = vectorize([](auto x) { return units::floor(x); });
const auto ceil = vectorize([](auto x) { return units::ceil(x); });
const auto trunc = vectorize([](auto x) { return units::trunc(x); });
const auto nearbyint = vectorize([](auto x) { return units::nearbyint(x); });
const auto rint = vectorize([](auto x) { return units::rint(x); });

// Complex numbers

const auto real = vectorize([](auto z) { return std::real(z); });
const auto imag = vectorize([](auto z) { return std::imag(z); });
const auto angle = vectorize([](auto z) { return std::arg(z); });
const auto norm = vectorize([](auto z) { return units::norm(z); });

const auto conj = vectorize([](auto z) {
    if constexpr (meta::is_complex_v<decltype(z)>) {
        return std::conj(z);
    } else {
        return z;
    }
});

const auto polar = vectorize([](auto r, auto theta) {
    using T = decltype(theta);
    static_assert(units::is_planar_angle<T>,
                  "polar theta argument must be of type units::planar_angle "
                  "(ex. radian or degree)");
    using rad = units::radian<typename T::value_type>;
    return std::polar(r, units::quantity_cast<rad>(theta).value());
});

// Rational routines

const auto gcd =
    vectorize([](auto m, auto n) scicpp_const { return std::gcd(m, n); });
const auto lcm =
    vectorize([](auto m, auto n) scicpp_const { return std::lcm(m, n); });

// Miscellaneous

const auto absolute = vectorize([](auto x) {
    if constexpr (meta::is_complex_v<decltype(x)>) {
        return std::abs(x);
    } else {
        return units::fabs(x);
    }
});

const auto sqrt = vectorize([](auto x) { return units::sqrt(x); });
const auto cbrt = vectorize([](auto x) { return units::cbrt(x); });

// pow

template <typename T1, typename T2>
constexpr auto pow(T1 &&x, T2 &&y) {
    if constexpr (meta::Iterable<T1>) {
        if constexpr (meta::Iterable<T2>) {
            return map([](auto a, auto b) { return std::pow(a, b); },
                       std::forward<T1>(x),
                       std::forward<T2>(y));
        } else { // y is a scalar
            return map([&](auto a) { return std::pow(a, y); },
                       std::forward<T1>(x));
        }
    } else { // x is a scalar
        if constexpr (meta::Iterable<T2>) {
            return map([&](auto a) { return std::pow(x, a); },
                       std::forward<T2>(y));
        } else {
            return std::pow(x, y);
        }
    }
}

template <intmax_t n, meta::Iterable T>
constexpr auto pow(T &&a) {
    return map([](auto x) { return units::pow<n>(x); }, std::forward<T>(a));
}

//---------------------------------------------------------------------------------
// midpoint and lerp
//---------------------------------------------------------------------------------

const auto midpoint =
    vectorize([](auto x, auto y) { return units::midpoint(x, y); });

// lerp c++ 20 => TODO use std lib directly
// Derived from https://github.com/llvm-mirror/libcxx/blob/master/include/cmath
// Handles physical quantities

template <meta::NonIterable T1, typename T2>
constexpr auto lerp(T1 a, T1 b, T2 t) noexcept {
    using ret_t = decltype(std::declval<T1>() * std::declval<T2>());

    if ((a <= T1{0} && b >= T1{0}) || (a >= T1{0} && b <= T1{0})) {
        return t * b + (T2{1} - t) * a;
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
    if (t == T2{1}) {
        return static_cast<ret_t>(b);
    }
#pragma GCC diagnostic pop

    const auto x = static_cast<ret_t>(a) + t * (b - a);

    if ((t > T2{1}) == (b > a)) {
        return b < x ? x : b;
    } else {
        return x < b ? x : b;
    }
}

template <meta::Iterable T1, typename T2>
constexpr auto lerp(T1 &&a, T1 &&b, T2 t) {
    return map([=](auto x, auto y) { return lerp(x, y, t); },
               std::forward<T1>(a),
               std::forward<T1>(b));
}

} // namespace scicpp

#endif // SCICPP_CORE_MATHS
