// SPDX-License-Identifier: MIT
// Copyright (c) 2019-2021 Thomas Vanderbruggen <th.vanderbruggen@gmail.com>

#ifndef SCICPP_CORE_CONSTANTS
#define SCICPP_CORE_CONSTANTS

#include "scicpp/core/units/quantity.hpp"
#include "scicpp/core/units/units.hpp"

namespace scicpp {

//---------------------------------------------------------------------------------
// Mathematical constants
//---------------------------------------------------------------------------------

template <typename T>
constexpr T pi = T(3.1415926535897932384626433832795029);

template <typename T>
constexpr T e = T(2.7182818284590452353602874713526625);

template <typename T>
constexpr T euler_gamma = T(0.5772156649015328606065120900824024310421);

// Apéry's constant zeta(3)
template <typename T>
constexpr T apery_cst = T(1.20205690315959428539);

//---------------------------------------------------------------------------------
// Physical constants
//
// From CODATA 2018
//---------------------------------------------------------------------------------

template <typename T>
struct physical_constants {
    static_assert(std::is_floating_point_v<T>);

    // Speed of light in vacuum
    static constexpr auto c = units::speed<T>(299792458);

    // Newtonian constant of gravitation
    using newton_grav_cst_qty = units::quantity_divide<
        units::volume<T>,
        units::quantity_multiply<
            units::mass<T>,
            units::quantity_multiply<units::time<T>, units::time<T>>>>;
    static constexpr auto G = newton_grav_cst_qty(6.67430E-11);

    // Planck constant
    using planck_cst_qty =
        units::quantity_multiply<units::energy<T>, units::time<T>>;
    static constexpr auto h = planck_cst_qty(T(6.62607015E-34));
    static constexpr auto hbar = h / (T{2} * pi<T>);

    // Elementary charge
    static constexpr auto e = units::electric_charge<T>(T(1.602176634E-19));

    // Electron mass
    static constexpr auto m_e = units::mass<T>(T(9.1093837015E-31));

    // Proton mass
    static constexpr auto m_p = units::mass<T>(T(1.67262192369E-27));

    // Fine-structure constant
    static constexpr auto alpha = T(7.2973525693E-3);

    // Vacuum magnetic permeability
    static constexpr auto mu0 = T{2} * alpha * h / (e * e * c);

    // Vacuum electric permittivity
    static constexpr auto epsilon0 = T{1} / (mu0 * c * c);

    // Josephson constant
    static constexpr auto K_J = T{2} * e / h;

    // von Klitzing constant
    static constexpr auto R_K = h / (e * e);

    // Magnetic flux quantum
    static constexpr auto Phi0 = T{1} / K_J;

    // Bohr magneton
    static constexpr auto muB = T{0.5} * e * hbar / m_e;

    // Nuclear magneton
    static constexpr auto muN = T{0.5} * e * hbar / m_p;

    // Rydberg constant
    static constexpr auto Rinf = T{0.5} * alpha * alpha * m_e * c / h;

    // Bohr radius
    static constexpr auto a0 = hbar / (alpha * m_e * c);

    // Boltzmann constant
    using boltzmann_cst_qty =
        units::quantity_divide<units::energy<T>, units::temperature<T>>;
    static constexpr auto k = boltzmann_cst_qty(1.380649E-23);
};

using phys_cst_f = physical_constants<float>;
using phys_cst = physical_constants<double>;
using phys_cst_l = physical_constants<long double>;

} // namespace scicpp

#endif // SCICPP_CORE_CONSTANTS