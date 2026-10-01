// SPDX-License-Identifier: MIT
// Copyright (c) 2019-2021 Thomas Vanderbruggen <th.vanderbruggen@gmail.com>

#ifndef SCICPP_CORE_CONSTANTS
#define SCICPP_CORE_CONSTANTS

#include "scicpp/core/macros.hpp"

#if SCICPP_HAS_UNITS

#include "scicpp/core/units/quantity.hpp"
#include "scicpp/core/units/units.hpp"

#include <concepts>

#endif // SCICPP_HAS_UNITS

namespace scicpp {

//---------------------------------------------------------------------------------
// Mathematical constants
//---------------------------------------------------------------------------------

template <typename T>
struct mathematical_constants;

template <std::floating_point T>
struct mathematical_constants<T> {
    static constexpr T pi =
        T(3.1415926535897932384626433832795029);

    static constexpr T e =
        T(2.7182818284590452353602874713526625);

    static constexpr T euler_gamma =
        T(0.5772156649015328606065120900824024310421);

    // Apéry's constant zeta(3)
    static constexpr T apery_cst =
        T(1.20205690315959428539);
};

#if SCICPP_HAS_UNITS

template <typename Q>
    requires units::is_quantity_v<Q>
struct mathematical_constants<Q> {
    using Rep = units::representation_t<Q>;

    static constexpr Q pi {
        mathematical_constants<Rep>::pi
    };

    static constexpr Q e {
        mathematical_constants<Rep>::e
    };

    static constexpr Q euler_gamma {
        mathematical_constants<Rep>::euler_gamma
    };

    // Apéry's constant zeta(3)
    static constexpr Q apery_cst {
        mathematical_constants<Rep>::apery_cst
    };
};

#endif // SCICPP_HAS_UNITS

template <typename T>
constexpr auto pi = mathematical_constants<T>::pi;

template <typename T>
constexpr auto e = mathematical_constants<T>::e;

template <typename T>
constexpr auto euler_gamma = mathematical_constants<T>::euler_gamma;

// Apéry's constant zeta(3)
template <typename T>
constexpr auto apery_cst = mathematical_constants<T>::apery_cst;

//---------------------------------------------------------------------------------
// Physical constants
//
// From CODATA 2018
//---------------------------------------------------------------------------------

#if SCICPP_HAS_UNITS

template <std::floating_point T>
using newton_grav_cst_qty = units::quantity_divide<
    units::volume<T>,
    units::quantity_multiply<
        units::mass<T>,
        units::quantity_multiply<units::time<T>, units::time<T>>>>;

struct codata2018 {
    template <std::floating_point T>
    static constexpr auto m_e() noexcept {
        return units::mass<T>(T(9.1093837015E-31));
    }

    template <std::floating_point T>
    static constexpr auto m_p() noexcept {
        return units::mass<T>(T(1.67262192369E-27));
    }

    template <std::floating_point T>
    static constexpr auto alpha() noexcept {
        return T(7.2973525693E-3);
    }

    template <std::floating_point T>
    static constexpr auto G() noexcept {
        return newton_grav_cst_qty<T>(6.67430E-11);
    }
};

struct codata2022 {
    template <std::floating_point T>
    static constexpr auto m_e() noexcept {
        return units::mass<T>(T(9.1093837139E-31));
    }

    template <std::floating_point T>
    static constexpr auto m_p() noexcept {
        return units::mass<T>(T(1.67262192595E-27));
    }

    template <std::floating_point T>
    static constexpr auto alpha() noexcept {
        return T(7.2973525643E-3);
    }

    template <std::floating_point T>
    static constexpr auto G() noexcept {
        return newton_grav_cst_qty<T>(6.67430E-11);
    }
};

template <std::floating_point T, class Codata>
struct physical_constants {
    using planck_cst_qty =
        units::quantity_multiply<units::energy<T>, units::time<T>>;

    using boltzmann_cst_qty =
        units::quantity_divide<units::energy<T>, units::temperature<T>>;

    // --------- Exact constants

    // Speed of light in vacuum (exact)
    static constexpr auto c = units::speed<T>(299792458);

    // Elementary charge (exact)
    static constexpr auto e = units::electric_charge<T>(T(1.602176634E-19));

    // Planck constant (exact)
    static constexpr auto h = planck_cst_qty(T(6.62607015E-34));
    static constexpr auto hbar = h / (T{2} * pi<T>);

    // Boltzmann constant (exact)
    static constexpr auto k = boltzmann_cst_qty(1.380649E-23);

    // --------- Other constants

    // Newtonian constant of gravitation
    static constexpr auto G = Codata::template G<T>();

    // Electron mass
    static constexpr auto m_e = Codata::template m_e<T>();

    // Proton mass
    static constexpr auto m_p = Codata::template m_p<T>();

    // Fine-structure constant
    static constexpr auto alpha = Codata::template alpha<T>();

    // --------- Derived constants

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
    static constexpr auto muB = static_cast<T>(0.5L) * e * hbar / m_e;

    // Nuclear magneton
    static constexpr auto muN = static_cast<T>(0.5L) * e * hbar / m_p;

    // Rydberg constant
    static constexpr auto Rinf =
        static_cast<T>(0.5L) * alpha * alpha * m_e * c / h;

    // Bohr radius
    static constexpr auto a0 = hbar / (alpha * m_e * c);
}; // struct physical_constants

using phys_cst_f = physical_constants<float, codata2022>;
using phys_cst = physical_constants<double, codata2022>;
using phys_cst_l = physical_constants<long double, codata2022>;

#endif // SCICPP_HAS_UNITS

} // namespace scicpp

#endif // SCICPP_CORE_CONSTANTS