// SPDX-License-Identifier: MIT
// Copyright (c) 2019-2021 Thomas Vanderbruggen <th.vanderbruggen@gmail.com>

#ifndef SCICPP_SIGNAL_WAVEFORMS
#define SCICPP_SIGNAL_WAVEFORMS

#include "scicpp/core/constants.hpp"
#include "scicpp/core/functional.hpp"
#include "scicpp/core/macros.hpp"
#include "scicpp/core/maths.hpp"
#include "scicpp/core/units/units.hpp"
#include "scicpp/polynomials/polynomial.hpp"

#include <array>
#include <cmath>
#include <cstdlib>
#include <type_traits>
#include <vector>

namespace scicpp::signal {

//---------------------------------------------------------------------------------
// unit_impulse
//---------------------------------------------------------------------------------

template <typename T, std::size_t N>
constexpr auto unit_impulse(std::size_t idx = 0) {
    std::array<T, N> res{};

    if (idx < N) {
        res[idx] = T(1);
    }

    return res;
}

template <typename T>
auto unit_impulse(std::size_t len, std::size_t idx = 0) {
    std::vector<T> res(len, T(0));

    if (idx < len) {
        res[idx] = T(1);
    }

    return res;
}

//---------------------------------------------------------------------------------
// sawtooth
//---------------------------------------------------------------------------------

template <class Array,
          typename T = typename std::remove_reference_t<Array>::value_type>
auto sawtooth(Array &&t, T width = T{1}) {
    scicpp_require(width >= T{0} && width <= T{1});

    return map(
        [width](auto t_) scicpp_pure {
            const auto tmod = std::fmod(t_, T{2} * pi<T>);

            if (tmod < width * T{2} * pi<T>) {
                scicpp_require(width > T{0});
                return tmod / (width * pi<T>)-T{1};
            } else {
                scicpp_require(width < T{1});
                return (width + T{1} - tmod / pi<T>) / (T{1} - width);
            }
        },
        std::forward<Array>(t));
}

//---------------------------------------------------------------------------------
// sweep_poly
//
// /!\ Polynomial order doesn't follow the legacy numpy.poly1d order
//     for the coefficients but the numpy.polynomial order.
//---------------------------------------------------------------------------------

template <class Array,
          class Poly,
          typename T = typename std::remove_reference_t<Array>::value_type>
auto sweep_poly(Array &&t,
                const Poly &poly,
                units::planar_angle<T> phi = units::planar_angle<T>(0)) {
    using namespace scicpp::operators;
    using namespace polynomial;

    return cos(units::radian<T>(T{2} * pi<T>) *
                   polyval(std::forward<Array>(t), polyint(poly)) +
               phi);
}

} // namespace scicpp::signal

#endif // SCICPP_SIGNAL_WAVEFORMS