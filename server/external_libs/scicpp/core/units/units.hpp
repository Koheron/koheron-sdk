// SPDX-License-Identifier: MIT
// Copyright (c) 2019-2021 Thomas Vanderbruggen <th.vanderbruggen@gmail.com>

#ifndef SCICPP_CORE_UNITS_UNITS
#define SCICPP_CORE_UNITS_UNITS

#include "scicpp/core/units/quantity.hpp"

#include <cmath>
#include <cstdio>
#include <ratio>
#include <type_traits>

namespace scicpp::units {

using DimSystem = dimensional_system<11>;

// ----------------------------------------------------------------------------
// Quantities
// ----------------------------------------------------------------------------

// Base quantities

template <typename T, typename Scale = scale<std::ratio<1>>>
using dimensionless = get_base_quantity<0, DimSystem, T, Scale>;

template <typename T, typename Scale = scale<std::ratio<1>>>
using length = get_base_quantity<1, DimSystem, T, Scale>;

template <typename T, typename Scale = scale<std::ratio<1>>>
using time = get_base_quantity<2, DimSystem, T, Scale>;

template <typename T, typename Scale = scale<std::ratio<1>>>
using mass = get_base_quantity<3, DimSystem, T, Scale>;

template <typename T, typename Scale = scale<std::ratio<1>>>
using electric_current = get_base_quantity<4, DimSystem, T, Scale>;

template <typename T,
          typename Scale = scale<std::ratio<1>>,
          typename Offset = std::ratio<0>>
using temperature = get_base_quantity<5, DimSystem, T, Scale, Offset>;

template <typename T, typename Scale = scale<std::ratio<1>>>
using amount_of_substance = get_base_quantity<6, DimSystem, T, Scale>;

template <typename T, typename Scale = scale<std::ratio<1>>>
using luminous_intensity = get_base_quantity<7, DimSystem, T, Scale>;

template <typename T, typename Scale = scale<std::ratio<1>>>
using planar_angle = get_base_quantity<8, DimSystem, T, Scale>;

template <typename T, typename Scale = scale<std::ratio<1>>>
using solid_angle = get_base_quantity<9, DimSystem, T, Scale>;

template <typename T, typename Scale = scale<std::ratio<1>>>
using data_quantity = get_base_quantity<10, DimSystem, T, Scale>;

// Derived quantities

// Speed = Length / Time
template <typename T, typename Scale = scale<std::ratio<1>>>
using speed = quantity_divide<length<T, Scale>, time<T>>;

// Acceleration = Speed / Time
template <typename T, typename Scale = scale<std::ratio<1>>>
using acceleration = quantity_divide<speed<T, Scale>, time<T>>;

// Angular velocity = Planar angle / Time
template <typename T, typename Scale = scale<std::ratio<1>>>
using angular_velocity = quantity_divide<planar_angle<T, Scale>, time<T>>;

// Momentum = Mass x Speed
template <typename T, typename Scale = scale<std::ratio<1>>>
using momentum = quantity_multiply<mass<T, Scale>, speed<T>>;

// Area = Length x Length
template <typename T, typename Scale = scale<std::ratio<1>>>
using area = quantity_multiply<length<T, Scale>, length<T>>;

// Volume = Area x Length
template <typename T, typename Scale = scale<std::ratio<1>>>
using volume = quantity_multiply<area<T, Scale>, length<T>>;

// Force = Mass x Acceleration
template <typename T, typename Scale = scale<std::ratio<1>>>
using force = quantity_multiply<mass<T, Scale>, acceleration<T>>;

// Power = Force x Speed
template <typename T, typename Scale = scale<std::ratio<1>>>
using power = quantity_multiply<force<T, Scale>, speed<T>>;

// Energy = Power x Time
template <typename T, typename Scale = scale<std::ratio<1>>>
using energy = quantity_multiply<power<T, Scale>, time<T>>;

template <typename T, typename Scale = scale<std::ratio<1>>>
using work = energy<T, Scale>;

// Pressure = Force / Area
template <typename T, typename Scale = scale<std::ratio<1>>>
using pressure = quantity_divide<force<T, Scale>, area<T>>;

// Frequency = 1 / Time
template <typename T, typename Scale = scale<std::ratio<1>>>
using frequency = quantity_divide<dimensionless<T, Scale>, time<T>>;

// Data rate = Quantity of data / Time
template <typename T, typename Scale = scale<std::ratio<1>>>
using data_rate = quantity_divide<data_quantity<T, Scale>, time<T>>;

// Quantities traits

#define QUANTITY_TRAIT(qty)                                                    \
    template <class T>                                                         \
    constexpr bool is_##qty##_impl() {                                         \
        using Tp = std::decay_t<T>;                                            \
        if constexpr (scicpp::meta::is_complex_v<Tp>) {                        \
            return is_##qty##_impl<typename Tp::value_type>();                 \
        } else {                                                               \
            if constexpr (is_quantity_v<Tp>) {                                 \
                return is_same_dimension<                                      \
                    Tp,                                                        \
                    qty<typename Tp::value_type, typename Tp::scal>>;          \
            } else {                                                           \
                return false;                                                  \
            }                                                                  \
        }                                                                      \
    }                                                                          \
                                                                               \
    template <class T>                                                         \
    constexpr bool is_##qty = is_##qty##_impl<T>();

QUANTITY_TRAIT(dimensionless)
QUANTITY_TRAIT(length)
QUANTITY_TRAIT(time)
QUANTITY_TRAIT(mass)
QUANTITY_TRAIT(electric_current)
QUANTITY_TRAIT(temperature)
QUANTITY_TRAIT(amount_of_substance)
QUANTITY_TRAIT(luminous_intensity)
QUANTITY_TRAIT(planar_angle)
QUANTITY_TRAIT(solid_angle)
QUANTITY_TRAIT(data_quantity)

QUANTITY_TRAIT(speed)
QUANTITY_TRAIT(acceleration)
QUANTITY_TRAIT(angular_velocity)
QUANTITY_TRAIT(momentum)
QUANTITY_TRAIT(area)
QUANTITY_TRAIT(volume)
QUANTITY_TRAIT(force)
QUANTITY_TRAIT(power)
QUANTITY_TRAIT(energy)
QUANTITY_TRAIT(work)
QUANTITY_TRAIT(pressure)
QUANTITY_TRAIT(frequency)

// ----------------------------------------------------------------------------
// Units
//
// An unit is the specialization of a quantity
// template for a given scale and offset.
// ----------------------------------------------------------------------------

#define SCICPP_CORE_UNITS_SET_LITERAL(quantity, literal, scale_ratio)          \
    constexpr auto operator""##literal(long double x) {                        \
        return quantity<double, scale<scale_ratio>>{static_cast<double>(x)};   \
    }                                                                          \
                                                                               \
    constexpr auto operator""##literal(unsigned long long x) {                 \
        return quantity<double, scale<scale_ratio>>{static_cast<double>(x)};   \
    }

#define SCICPP_CORE_UNITS_SET_LITERAL_RATIO(quantity, literal, num, den)       \
    constexpr auto operator""##literal(long double x) {                        \
        return quantity<double, scale<std::ratio<(num), (den)>>>{              \
            static_cast<double>(x)};                                           \
    }                                                                          \
                                                                               \
    constexpr auto operator""##literal(unsigned long long x) {                 \
        return quantity<double, scale<std::ratio<(num), (den)>>>{              \
            static_cast<double>(x)};                                           \
    }

#define SCICPP_CORE_UNITS_SET_LITERAL_RATIO2(                                  \
    quantity, literal, num1, den1, num2, den2)                                 \
    constexpr auto operator""##literal(long double x) {                        \
        return quantity<double,                                                \
                        scale<std::ratio<(num1), (den1)>>,                     \
                        std::ratio<(num2), (den2)>>{static_cast<double>(x)};   \
    }                                                                          \
                                                                               \
    constexpr auto operator""##literal(unsigned long long x) {                 \
        return quantity<double,                                                \
                        scale<std::ratio<(num1), (den1)>>,                     \
                        std::ratio<(num2), (den2)>>{static_cast<double>(x)};   \
    }

#define SCICPP_CORE_UNITS_DEFINE_PREFIXES_ALIAS(quantity, base_unit)           \
    template <typename T = double>                                             \
    using atto##base_unit = quantity<T, scale<std::atto>>;                     \
    template <typename T = double>                                             \
    using femto##base_unit = quantity<T, scale<std::femto>>;                   \
    template <typename T = double>                                             \
    using pico##base_unit = quantity<T, scale<std::pico>>;                     \
    template <typename T = double>                                             \
    using nano##base_unit = quantity<T, scale<std::nano>>;                     \
    template <typename T = double>                                             \
    using micro##base_unit = quantity<T, scale<std::micro>>;                   \
    template <typename T = double>                                             \
    using milli##base_unit = quantity<T, scale<std::milli>>;                   \
    template <typename T = double>                                             \
    using centi##base_unit = quantity<T, scale<std::centi>>;                   \
    template <typename T = double>                                             \
    using deci##base_unit = quantity<T, scale<std::deci>>;                     \
    template <typename T = double>                                             \
    using base_unit = quantity<T>;                                             \
    template <typename T = double>                                             \
    using deca##base_unit = quantity<T, scale<std::deca>>;                     \
    template <typename T = double>                                             \
    using hecto##base_unit = quantity<T, scale<std::hecto>>;                   \
    template <typename T = double>                                             \
    using kilo##base_unit = quantity<T, scale<std::kilo>>;                     \
    template <typename T = double>                                             \
    using mega##base_unit = quantity<T, scale<std::mega>>;                     \
    template <typename T = double>                                             \
    using giga##base_unit = quantity<T, scale<std::giga>>;                     \
    template <typename T = double>                                             \
    using tera##base_unit = quantity<T, scale<std::tera>>;                     \
    template <typename T = double>                                             \
    using peta##base_unit = quantity<T, scale<std::peta>>;                     \
    template <typename T = double>                                             \
    using exa##base_unit = quantity<T, scale<std::exa>>;

// Primary quantities

// ----------------------------------------------------------------------------
// Length
// ----------------------------------------------------------------------------

SCICPP_CORE_UNITS_DEFINE_PREFIXES_ALIAS(length, meter)

template <typename T = double>
using angstrom = length<T, scale<std::ratio<1, 10000000000>>>;

// Imperial

template <typename T = double>
using inch = length<T, scale<std::ratio<254, 10000>>>;

template <typename T = double>
using foot = length<T, scale<std::ratio<3048, 10000>>>;

template <typename T = double>
using yard = length<T, scale<std::ratio<9144, 10000>>>;

template <typename T = double>
using mile = length<T, scale<std::ratio<25146000, 15625>>>;

// Marine

template <typename T = double>
using nautical_mile = length<T, scale<std::ratio<1852>>>;

// Astronomy

template <typename T = double>
using astronomical_unit = length<T, scale<std::ratio<149597870700>>>;

template <typename T = double>
using light_year = length<T, scale<std::ratio<9460730472580800>>>;

template <typename T = double>
using parsec = length<T, scale<std::ratio<30856775814671900>>>;

namespace literals {

SCICPP_CORE_UNITS_SET_LITERAL(length, _am, std::atto)
SCICPP_CORE_UNITS_SET_LITERAL(length, _fm, std::femto)
SCICPP_CORE_UNITS_SET_LITERAL(length, _pm, std::pico)
SCICPP_CORE_UNITS_SET_LITERAL(length, _nm, std::nano)
SCICPP_CORE_UNITS_SET_LITERAL(length, _um, std::micro)
SCICPP_CORE_UNITS_SET_LITERAL(length, _mm, std::milli)
SCICPP_CORE_UNITS_SET_LITERAL(length, _cm, std::centi)
SCICPP_CORE_UNITS_SET_LITERAL(length, _m, std::ratio<1>)
SCICPP_CORE_UNITS_SET_LITERAL(length, _km, std::kilo)
SCICPP_CORE_UNITS_SET_LITERAL(length, _Mm, std::mega)
SCICPP_CORE_UNITS_SET_LITERAL(length, _Gm, std::giga)
SCICPP_CORE_UNITS_SET_LITERAL(length, _Tm, std::tera)
SCICPP_CORE_UNITS_SET_LITERAL(length, _Pm, std::peta)
SCICPP_CORE_UNITS_SET_LITERAL(length, _Em, std::exa)

SCICPP_CORE_UNITS_SET_LITERAL_RATIO(length, _angstrom, 1, 10000000000)
SCICPP_CORE_UNITS_SET_LITERAL_RATIO(length, _mi, 25146000, 15625)
SCICPP_CORE_UNITS_SET_LITERAL_RATIO(length, _in, 254, 10000)
SCICPP_CORE_UNITS_SET_LITERAL_RATIO(length, _ft, 3048, 10000)
SCICPP_CORE_UNITS_SET_LITERAL_RATIO(length, _yd, 9144, 10000)
SCICPP_CORE_UNITS_SET_LITERAL_RATIO(length, _nmi, 1852, 1)

SCICPP_CORE_UNITS_SET_LITERAL_RATIO(length, _au, 149597870700, 1)
SCICPP_CORE_UNITS_SET_LITERAL_RATIO(length, _ly, 9460730472580800, 1)
SCICPP_CORE_UNITS_SET_LITERAL_RATIO(length, _pc, 30856775814671900, 1)

} // namespace literals

// ----------------------------------------------------------------------------
// Time
// ----------------------------------------------------------------------------

SCICPP_CORE_UNITS_DEFINE_PREFIXES_ALIAS(time, second)

template <typename T = double>
using minute = time<T, scale<std::ratio<60>>>;

template <typename T = double>
using hour = time<T, scale<std::ratio<3600>>>;

template <typename T = double>
using day = time<T, scale<std::ratio<86400>>>;

template <typename T = double>
using week = time<T, scale<std::ratio<604800>>>;

namespace literals {

SCICPP_CORE_UNITS_SET_LITERAL(time, _as, std::atto)
SCICPP_CORE_UNITS_SET_LITERAL(time, _fs, std::femto)
SCICPP_CORE_UNITS_SET_LITERAL(time, _ps, std::pico)
SCICPP_CORE_UNITS_SET_LITERAL(time, _ns, std::nano)
SCICPP_CORE_UNITS_SET_LITERAL(time, _us, std::micro)
SCICPP_CORE_UNITS_SET_LITERAL(time, _ms, std::milli)
SCICPP_CORE_UNITS_SET_LITERAL(time, _s, std::ratio<1>)
SCICPP_CORE_UNITS_SET_LITERAL(time, _ks, std::kilo)
SCICPP_CORE_UNITS_SET_LITERAL(time, _Ms, std::mega)
SCICPP_CORE_UNITS_SET_LITERAL(time, _Gs, std::giga)
SCICPP_CORE_UNITS_SET_LITERAL(time, _Ts, std::tera)
SCICPP_CORE_UNITS_SET_LITERAL(time, _Ps, std::peta)
SCICPP_CORE_UNITS_SET_LITERAL(time, _Es, std::exa)

SCICPP_CORE_UNITS_SET_LITERAL_RATIO(time, _min, 60, 1)
SCICPP_CORE_UNITS_SET_LITERAL_RATIO(time, _h, 3600, 1)
SCICPP_CORE_UNITS_SET_LITERAL_RATIO(time, _day, 86400, 1)
SCICPP_CORE_UNITS_SET_LITERAL_RATIO(time, _week, 604800, 1)

} // namespace literals

// ----------------------------------------------------------------------------
// Mass
// ----------------------------------------------------------------------------

template <typename T = double>
using femtogram = mass<T, scale<std::atto>>;

template <typename T = double>
using picogram = mass<T, scale<std::femto>>;

template <typename T = double>
using nanogram = mass<T, scale<std::pico>>;

template <typename T = double>
using microgram = mass<T, scale<std::nano>>;

template <typename T = double>
using milligram = mass<T, scale<std::micro>>;

template <typename T = double>
using gram = mass<T, scale<std::milli>>;

template <typename T = double>
using kilogram = mass<T>;

template <typename T = double>
using tonne = mass<T, scale<std::kilo>>;

template <typename T = double>
using kilotonne = mass<T, scale<std::mega>>;

template <typename T = double>
using megatonne = mass<T, scale<std::giga>>;

template <typename T = double>
using gigatonne = mass<T, scale<std::tera>>;

template <typename T = double>
using teratonne = mass<T, scale<std::exa>>;

namespace literals {

SCICPP_CORE_UNITS_SET_LITERAL(mass, _fg, std::atto)
SCICPP_CORE_UNITS_SET_LITERAL(mass, _pg, std::femto)
SCICPP_CORE_UNITS_SET_LITERAL(mass, _ng, std::pico)
SCICPP_CORE_UNITS_SET_LITERAL(mass, _ug, std::nano)
SCICPP_CORE_UNITS_SET_LITERAL(mass, _mg, std::micro)
SCICPP_CORE_UNITS_SET_LITERAL(mass, _g, std::milli)
SCICPP_CORE_UNITS_SET_LITERAL(mass, _kg, std::ratio<1>)
SCICPP_CORE_UNITS_SET_LITERAL(mass, _t, std::kilo)
SCICPP_CORE_UNITS_SET_LITERAL(mass, _kt, std::mega)
SCICPP_CORE_UNITS_SET_LITERAL(mass, _Mt, std::giga)
SCICPP_CORE_UNITS_SET_LITERAL(mass, _Gt, std::tera)
SCICPP_CORE_UNITS_SET_LITERAL(mass, _Tt, std::exa)

} // namespace literals

// ----------------------------------------------------------------------------
// Electric current
// ----------------------------------------------------------------------------

SCICPP_CORE_UNITS_DEFINE_PREFIXES_ALIAS(electric_current, ampere)

namespace literals {

SCICPP_CORE_UNITS_SET_LITERAL(electric_current, _aA, std::atto)
SCICPP_CORE_UNITS_SET_LITERAL(electric_current, _fA, std::femto)
SCICPP_CORE_UNITS_SET_LITERAL(electric_current, _pA, std::pico)
SCICPP_CORE_UNITS_SET_LITERAL(electric_current, _nA, std::nano)
SCICPP_CORE_UNITS_SET_LITERAL(electric_current, _uA, std::micro)
SCICPP_CORE_UNITS_SET_LITERAL(electric_current, _mA, std::milli)
SCICPP_CORE_UNITS_SET_LITERAL(electric_current, _A, std::ratio<1>)
SCICPP_CORE_UNITS_SET_LITERAL(electric_current, _kA, std::kilo)
SCICPP_CORE_UNITS_SET_LITERAL(electric_current, _MA, std::mega)
SCICPP_CORE_UNITS_SET_LITERAL(electric_current, _GA, std::giga)
SCICPP_CORE_UNITS_SET_LITERAL(electric_current, _TA, std::tera)
SCICPP_CORE_UNITS_SET_LITERAL(electric_current, _PA, std::peta)
SCICPP_CORE_UNITS_SET_LITERAL(electric_current, _EA, std::exa)

} // namespace literals

// ----------------------------------------------------------------------------
// Temperature
// ----------------------------------------------------------------------------

SCICPP_CORE_UNITS_DEFINE_PREFIXES_ALIAS(temperature, kelvin)

template <typename T = double>
using celsius = temperature<T, scale<std::ratio<1>>, std::ratio<27315, 100>>;

template <typename T = double>
using fahrhenheit =
    temperature<T, scale<std::ratio<5, 9>>, std::ratio<45967, 180>>;

namespace literals {

SCICPP_CORE_UNITS_SET_LITERAL(temperature, _aK, std::atto)
SCICPP_CORE_UNITS_SET_LITERAL(temperature, _fK, std::femto)
SCICPP_CORE_UNITS_SET_LITERAL(temperature, _pK, std::pico)
SCICPP_CORE_UNITS_SET_LITERAL(temperature, _nK, std::nano)
SCICPP_CORE_UNITS_SET_LITERAL(temperature, _uK, std::micro)
SCICPP_CORE_UNITS_SET_LITERAL(temperature, _mK, std::milli)
SCICPP_CORE_UNITS_SET_LITERAL(temperature, _K, std::ratio<1>)
SCICPP_CORE_UNITS_SET_LITERAL(temperature, _kK, std::kilo)
SCICPP_CORE_UNITS_SET_LITERAL(temperature, _MK, std::mega)
SCICPP_CORE_UNITS_SET_LITERAL(temperature, _GK, std::giga)
SCICPP_CORE_UNITS_SET_LITERAL(temperature, _TK, std::tera)
SCICPP_CORE_UNITS_SET_LITERAL(temperature, _PK, std::peta)
SCICPP_CORE_UNITS_SET_LITERAL(temperature, _EK, std::exa)

SCICPP_CORE_UNITS_SET_LITERAL_RATIO2(temperature, _degC, 1, 1, 27315, 100)
SCICPP_CORE_UNITS_SET_LITERAL_RATIO2(temperature, _degF, 5, 9, 45967, 180)

} // namespace literals

// ----------------------------------------------------------------------------
// Amount of substance
// ----------------------------------------------------------------------------

SCICPP_CORE_UNITS_DEFINE_PREFIXES_ALIAS(amount_of_substance, mole)

namespace literals {

SCICPP_CORE_UNITS_SET_LITERAL(amount_of_substance, _amol, std::atto)
SCICPP_CORE_UNITS_SET_LITERAL(amount_of_substance, _fmol, std::femto)
SCICPP_CORE_UNITS_SET_LITERAL(amount_of_substance, _pmol, std::pico)
SCICPP_CORE_UNITS_SET_LITERAL(amount_of_substance, _nmol, std::nano)
SCICPP_CORE_UNITS_SET_LITERAL(amount_of_substance, _umol, std::micro)
SCICPP_CORE_UNITS_SET_LITERAL(amount_of_substance, _mmol, std::milli)
SCICPP_CORE_UNITS_SET_LITERAL(amount_of_substance, _mol, std::ratio<1>)
SCICPP_CORE_UNITS_SET_LITERAL(amount_of_substance, _kmol, std::kilo)
SCICPP_CORE_UNITS_SET_LITERAL(amount_of_substance, _Mmol, std::mega)
SCICPP_CORE_UNITS_SET_LITERAL(amount_of_substance, _Gmol, std::giga)
SCICPP_CORE_UNITS_SET_LITERAL(amount_of_substance, _Tmol, std::tera)
SCICPP_CORE_UNITS_SET_LITERAL(amount_of_substance, _Pmol, std::peta)
SCICPP_CORE_UNITS_SET_LITERAL(amount_of_substance, _Emol, std::exa)

} // namespace literals

// ----------------------------------------------------------------------------
// Luminous intensity
// ----------------------------------------------------------------------------

SCICPP_CORE_UNITS_DEFINE_PREFIXES_ALIAS(luminous_intensity, candela)

namespace literals {

SCICPP_CORE_UNITS_SET_LITERAL(luminous_intensity, _aCd, std::atto)
SCICPP_CORE_UNITS_SET_LITERAL(luminous_intensity, _fCd, std::femto)
SCICPP_CORE_UNITS_SET_LITERAL(luminous_intensity, _pCd, std::pico)
SCICPP_CORE_UNITS_SET_LITERAL(luminous_intensity, _nCd, std::nano)
SCICPP_CORE_UNITS_SET_LITERAL(luminous_intensity, _uCd, std::micro)
SCICPP_CORE_UNITS_SET_LITERAL(luminous_intensity, _mCd, std::milli)
SCICPP_CORE_UNITS_SET_LITERAL(luminous_intensity, _Cd, std::ratio<1>)
SCICPP_CORE_UNITS_SET_LITERAL(luminous_intensity, _kCd, std::kilo)
SCICPP_CORE_UNITS_SET_LITERAL(luminous_intensity, _MCd, std::mega)
SCICPP_CORE_UNITS_SET_LITERAL(luminous_intensity, _GCd, std::giga)
SCICPP_CORE_UNITS_SET_LITERAL(luminous_intensity, _TCd, std::tera)
SCICPP_CORE_UNITS_SET_LITERAL(luminous_intensity, _PCd, std::peta)
SCICPP_CORE_UNITS_SET_LITERAL(luminous_intensity, _ECd, std::exa)

} // namespace literals

// ----------------------------------------------------------------------------
// Planar angle
// ----------------------------------------------------------------------------

SCICPP_CORE_UNITS_DEFINE_PREFIXES_ALIAS(planar_angle, radian)

using pi_ratio =
    std::ratio<21053343141, 6701487259>; // Precise to 21 decimal places
using deg_to_rad_ratio = std::ratio_divide<pi_ratio, std::ratio<180>>;
using turn_to_rad_ratio = std::ratio_multiply<std::ratio<2>, pi_ratio>;

template <typename T = double>
using degree = planar_angle<T, scale<deg_to_rad_ratio>>;

template <typename T = double>
using turn = planar_angle<T, scale<turn_to_rad_ratio>>;

namespace literals {

SCICPP_CORE_UNITS_SET_LITERAL(planar_angle, _arad, std::atto)
SCICPP_CORE_UNITS_SET_LITERAL(planar_angle, _frad, std::femto)
SCICPP_CORE_UNITS_SET_LITERAL(planar_angle, _prad, std::pico)
SCICPP_CORE_UNITS_SET_LITERAL(planar_angle, _nrad, std::nano)
SCICPP_CORE_UNITS_SET_LITERAL(planar_angle, _urad, std::micro)
SCICPP_CORE_UNITS_SET_LITERAL(planar_angle, _mrad, std::milli)
SCICPP_CORE_UNITS_SET_LITERAL(planar_angle, _rad, std::ratio<1>)
SCICPP_CORE_UNITS_SET_LITERAL(planar_angle, _krad, std::kilo)
SCICPP_CORE_UNITS_SET_LITERAL(planar_angle, _Mrad, std::mega)
SCICPP_CORE_UNITS_SET_LITERAL(planar_angle, _Grad, std::giga)
SCICPP_CORE_UNITS_SET_LITERAL(planar_angle, _Trad, std::tera)
SCICPP_CORE_UNITS_SET_LITERAL(planar_angle, _Prad, std::peta)
SCICPP_CORE_UNITS_SET_LITERAL(planar_angle, _Erad, std::exa)

SCICPP_CORE_UNITS_SET_LITERAL_RATIO(planar_angle,
                                    _deg,
                                    deg_to_rad_ratio::num,
                                    deg_to_rad_ratio::den)

SCICPP_CORE_UNITS_SET_LITERAL_RATIO(planar_angle,
                                    _turn,
                                    turn_to_rad_ratio::num,
                                    turn_to_rad_ratio::den)

} // namespace literals

// ----------------------------------------------------------------------------
// Solid angle
// ----------------------------------------------------------------------------

SCICPP_CORE_UNITS_DEFINE_PREFIXES_ALIAS(solid_angle, steradian)

namespace literals {

SCICPP_CORE_UNITS_SET_LITERAL(solid_angle, _asr, std::atto)
SCICPP_CORE_UNITS_SET_LITERAL(solid_angle, _fsr, std::femto)
SCICPP_CORE_UNITS_SET_LITERAL(solid_angle, _psr, std::pico)
SCICPP_CORE_UNITS_SET_LITERAL(solid_angle, _nsr, std::nano)
SCICPP_CORE_UNITS_SET_LITERAL(solid_angle, _usr, std::micro)
SCICPP_CORE_UNITS_SET_LITERAL(solid_angle, _msr, std::milli)
SCICPP_CORE_UNITS_SET_LITERAL(solid_angle, _sr, std::ratio<1>)
SCICPP_CORE_UNITS_SET_LITERAL(solid_angle, _ksr, std::kilo)
SCICPP_CORE_UNITS_SET_LITERAL(solid_angle, _Msr, std::mega)
SCICPP_CORE_UNITS_SET_LITERAL(solid_angle, _Gsr, std::giga)
SCICPP_CORE_UNITS_SET_LITERAL(solid_angle, _Tsr, std::tera)
SCICPP_CORE_UNITS_SET_LITERAL(solid_angle, _Psr, std::peta)
SCICPP_CORE_UNITS_SET_LITERAL(solid_angle, _Esr, std::exa)

} // namespace literals

// ----------------------------------------------------------------------------
// Data quantity
// ----------------------------------------------------------------------------

template <typename T = double>
using bit = data_quantity<T, scale<std::ratio<1>>>;

template <typename T = double>
using shannon = bit<T>;

template <typename T = double>
using nibble = data_quantity<T, scale<std::ratio<4>>>;

template <typename T = double>
using byte = data_quantity<T, scale<std::ratio<8>>>;

template <typename T = double>
using kibibyte = data_quantity<T, scale<std::ratio<8LL * 1024LL>>>;

template <typename T = double>
using mebibyte = data_quantity<T, scale<std::ratio<8LL * 1048576LL>>>;

template <typename T = double>
using gibibyte = data_quantity<T, scale<std::ratio<8LL * 1073741824LL>>>;

template <typename T = double>
using tebibyte = data_quantity<T, scale<std::ratio<8LL * 1099511627776LL>>>;

template <typename T = double>
using pebibyte = data_quantity<T, scale<std::ratio<8LL * 1125899906842624LL>>>;

namespace literals {

SCICPP_CORE_UNITS_SET_LITERAL(data_quantity, _Sh, std::ratio<1>)
SCICPP_CORE_UNITS_SET_LITERAL(data_quantity, _b, std::ratio<1>)
SCICPP_CORE_UNITS_SET_LITERAL_RATIO(data_quantity, _B, 8, 1)
SCICPP_CORE_UNITS_SET_LITERAL_RATIO(data_quantity, _kiB, 8LL * 1024LL, 1)
SCICPP_CORE_UNITS_SET_LITERAL_RATIO(data_quantity, _MiB, 8LL * 1048576LL, 1)
SCICPP_CORE_UNITS_SET_LITERAL_RATIO(data_quantity, _GiB, 8LL * 1073741824LL, 1)
SCICPP_CORE_UNITS_SET_LITERAL_RATIO(data_quantity,
                                    _TiB,
                                    8LL * 1099511627776LL,
                                    1)
SCICPP_CORE_UNITS_SET_LITERAL_RATIO(data_quantity,
                                    _PiB,
                                    8LL * 1125899906842624LL,
                                    1)

} // namespace literals

// ----------------------------------------------------------------------------
// Speed
// ----------------------------------------------------------------------------

SCICPP_CORE_UNITS_DEFINE_PREFIXES_ALIAS(speed, meter_per_second)

template <typename T = double>
using kilometer_per_hour = speed<T, scale<std::ratio<3600, 1000>>>;

template <typename T = double>
using knot = speed<T, scale<std::ratio<66672, 10000>>>;

template <typename T = double>
using mile_per_hour = speed<T, scale<std::ratio<44704, 100000>>>;

namespace literals {

SCICPP_CORE_UNITS_SET_LITERAL(speed, _am_per_s, std::atto)
SCICPP_CORE_UNITS_SET_LITERAL(speed, _fm_per_s, std::femto)
SCICPP_CORE_UNITS_SET_LITERAL(speed, _pm_per_s, std::pico)
SCICPP_CORE_UNITS_SET_LITERAL(speed, _nm_per_s, std::nano)
SCICPP_CORE_UNITS_SET_LITERAL(speed, _um_per_s, std::micro)
SCICPP_CORE_UNITS_SET_LITERAL(speed, _mm_per_s, std::milli)
SCICPP_CORE_UNITS_SET_LITERAL(speed, _m_per_s, std::ratio<1>)
SCICPP_CORE_UNITS_SET_LITERAL(speed, _km_per_s, std::kilo)
SCICPP_CORE_UNITS_SET_LITERAL(speed, _Mm_per_s, std::mega)
SCICPP_CORE_UNITS_SET_LITERAL(speed, _Gm_per_s, std::giga)
SCICPP_CORE_UNITS_SET_LITERAL(speed, _Tm_per_s, std::tera)
SCICPP_CORE_UNITS_SET_LITERAL(speed, _Pm_per_s, std::peta)
SCICPP_CORE_UNITS_SET_LITERAL(speed, _Em_per_s, std::exa)

SCICPP_CORE_UNITS_SET_LITERAL_RATIO(speed, _km_per_h, 3600, 1000)
SCICPP_CORE_UNITS_SET_LITERAL_RATIO(speed, _kn, 66672, 10000)
SCICPP_CORE_UNITS_SET_LITERAL_RATIO(speed, _mph, 44704, 100000)

} // namespace literals

// ----------------------------------------------------------------------------
// Acceleration
// ----------------------------------------------------------------------------

SCICPP_CORE_UNITS_DEFINE_PREFIXES_ALIAS(acceleration, meter_per_second_squared)

template <typename T = double>
using galileo = acceleration<T, scale<std::ratio<1, 100>>>;

template <typename T = double>
using milligalileo = acceleration<T, scale<std::ratio<1, 100000>>>;

template <typename T = double>
using microgalileo = acceleration<T, scale<std::ratio<1, 100000000>>>;

template <typename T = double>
using nanogalileo = acceleration<T, scale<std::ratio<1, 100000000000>>>;

namespace literals {

SCICPP_CORE_UNITS_SET_LITERAL(acceleration, _am_per_s2, std::atto)
SCICPP_CORE_UNITS_SET_LITERAL(acceleration, _fm_per_s2, std::femto)
SCICPP_CORE_UNITS_SET_LITERAL(acceleration, _pm_per_s2, std::pico)
SCICPP_CORE_UNITS_SET_LITERAL(acceleration, _nm_per_s2, std::nano)
SCICPP_CORE_UNITS_SET_LITERAL(acceleration, _um_per_s2, std::micro)
SCICPP_CORE_UNITS_SET_LITERAL(acceleration, _mm_per_s2, std::milli)
SCICPP_CORE_UNITS_SET_LITERAL(acceleration, _m_per_s2, std::ratio<1>)
SCICPP_CORE_UNITS_SET_LITERAL(acceleration, _km_per_s2, std::kilo)
SCICPP_CORE_UNITS_SET_LITERAL(acceleration, _Mm_per_s2, std::mega)
SCICPP_CORE_UNITS_SET_LITERAL(acceleration, _Gm_per_s2, std::giga)
SCICPP_CORE_UNITS_SET_LITERAL(acceleration, _Tm_per_s2, std::tera)
SCICPP_CORE_UNITS_SET_LITERAL(acceleration, _Pm_per_s2, std::peta)
SCICPP_CORE_UNITS_SET_LITERAL(acceleration, _Em_per_s2, std::exa)

SCICPP_CORE_UNITS_SET_LITERAL_RATIO(acceleration, _Gal, 1, 100)
SCICPP_CORE_UNITS_SET_LITERAL_RATIO(acceleration, _mGal, 1, 100000)
SCICPP_CORE_UNITS_SET_LITERAL_RATIO(acceleration, _uGal, 1, 100000000)
SCICPP_CORE_UNITS_SET_LITERAL_RATIO(acceleration, _nGal, 1, 100000000000)

} // namespace literals

// ----------------------------------------------------------------------------
// area
// ----------------------------------------------------------------------------

// SI

template <typename T = double>
using square_micrometer = area<T, scale<std::pico>>;

template <typename T = double>
using square_millimeter = area<T, scale<std::micro>>;

template <typename T = double>
using square_centimeter = area<T, scale<std::ratio<1, 10000>>>;

template <typename T = double>
using square_meter = area<T>;

template <typename T = double>
using square_kilometer = area<T, scale<std::mega>>;

template <typename T = double>
using square_megameter = area<T, scale<std::tera>>;

// Imperial

template <typename T = double>
using square_inch = area<T, scale<std::ratio<64516, 100000000>>>;

template <typename T = double>
using square_foot = area<T, scale<std::ratio<9290304, 100000000>>>;

template <typename T = double>
using square_yard = area<T, scale<std::ratio<83612736, 100000000>>>;

template <typename T = double>
using square_mile = area<T, scale<std::ratio<2589988110336, 1000000>>>;

// Non-SI metric

template <typename T = double>
using are = area<T, scale<std::ratio<100>>>;

template <typename T = double>
using hectare = area<T, scale<std::ratio<10000>>>;

namespace literals {

SCICPP_CORE_UNITS_SET_LITERAL(area, _um2, std::pico)
SCICPP_CORE_UNITS_SET_LITERAL(area, _mm2, std::micro)
SCICPP_CORE_UNITS_SET_LITERAL_RATIO(area, _cm2, 1, 10000)
SCICPP_CORE_UNITS_SET_LITERAL(area, _m2, std::ratio<1>)
SCICPP_CORE_UNITS_SET_LITERAL(area, _km2, std::mega)
SCICPP_CORE_UNITS_SET_LITERAL(area, _Mm2, std::tera)

SCICPP_CORE_UNITS_SET_LITERAL_RATIO(area, _in2, 64516, 100000000)
SCICPP_CORE_UNITS_SET_LITERAL_RATIO(area, _ft2, 9290304, 100000000)
SCICPP_CORE_UNITS_SET_LITERAL_RATIO(area, _yd2, 83612736, 100000000)
SCICPP_CORE_UNITS_SET_LITERAL_RATIO(area, _mi2, 2589988110336, 1000000)

SCICPP_CORE_UNITS_SET_LITERAL_RATIO(area, _a, 100, 1)
SCICPP_CORE_UNITS_SET_LITERAL_RATIO(area, _ha, 10000, 1)

} // namespace literals

// ----------------------------------------------------------------------------
// Volume
// ----------------------------------------------------------------------------

// SI

template <typename T = double>
using cubic_millimeter = volume<T, scale<std::nano>>;

template <typename T = double>
using cubic_centimeter = volume<T, scale<std::micro>>;

template <typename T = double>
using cubic_meter = volume<T>;

template <typename T = double>
using cubic_kilometer = volume<T, scale<std::giga>>;

// Metric

template <typename T = double>
using microliter = volume<T, scale<std::nano>>;

template <typename T = double>
using milliliter = volume<T, scale<std::micro>>;

template <typename T = double>
using liter = volume<T, scale<std::milli>>;

template <typename T = double>
using hectoliter = volume<T, scale<std::ratio<1, 10>>>;

// Imperial

template <typename T = double>
using cubic_inch = volume<T, scale<std::ratio<16387064, 1000000000000>>>;

template <typename T = double>
using cubic_foot = volume<T, scale<std::ratio<28316846592, 1000000000000>>>;

template <typename T = double>
using cubic_yard = volume<T, scale<std::ratio<764554857984, 1000000000000>>>;

template <typename T = double>
using cubic_mile =
    volume<T, scale<std::ratio<4168181825440579584, 1000000000>>>;

namespace literals {

SCICPP_CORE_UNITS_SET_LITERAL(volume, _mm3, std::nano)
SCICPP_CORE_UNITS_SET_LITERAL(volume, _cm3, std::micro)
SCICPP_CORE_UNITS_SET_LITERAL(volume, _m3, std::ratio<1>)
SCICPP_CORE_UNITS_SET_LITERAL(volume, _km3, std::giga)

SCICPP_CORE_UNITS_SET_LITERAL(volume, _uL, std::nano)
SCICPP_CORE_UNITS_SET_LITERAL(volume, _mL, std::micro)
SCICPP_CORE_UNITS_SET_LITERAL(volume, _L, std::milli)
SCICPP_CORE_UNITS_SET_LITERAL_RATIO(volume, _hL, 1, 10)

SCICPP_CORE_UNITS_SET_LITERAL_RATIO(volume, _in3, 16387064, 1000000000000)
SCICPP_CORE_UNITS_SET_LITERAL_RATIO(volume, _ft3, 28316846592, 1000000000000)
SCICPP_CORE_UNITS_SET_LITERAL_RATIO(volume, _yd3, 764554857984, 1000000000000)
SCICPP_CORE_UNITS_SET_LITERAL_RATIO(volume,
                                    _mi3,
                                    4168181825440579584,
                                    1000000000)

} // namespace literals

// ----------------------------------------------------------------------------
// Force
// ----------------------------------------------------------------------------

SCICPP_CORE_UNITS_DEFINE_PREFIXES_ALIAS(force, newton)

// Imperial

template <typename T = double>
using pound_force =
    volume<T, scale<std::ratio<44482216152605, 10000000000000>>>;

template <typename T = double>
using poundal = volume<T, scale<std::ratio<138254954376, 1000000000000>>>;

// Misc

template <typename T = double>
using kilogram_force = volume<T, scale<std::ratio<980665, 100000>>>;

template <typename T = double>
using dyne = volume<T, scale<std::ratio<1, 100000>>>;

namespace literals {

SCICPP_CORE_UNITS_SET_LITERAL(force, _aN, std::atto)
SCICPP_CORE_UNITS_SET_LITERAL(force, _fN, std::femto)
SCICPP_CORE_UNITS_SET_LITERAL(force, _pN, std::pico)
SCICPP_CORE_UNITS_SET_LITERAL(force, _nN, std::nano)
SCICPP_CORE_UNITS_SET_LITERAL(force, _uN, std::micro)
SCICPP_CORE_UNITS_SET_LITERAL(force, _mN, std::milli)
SCICPP_CORE_UNITS_SET_LITERAL(force, _N, std::ratio<1>)
SCICPP_CORE_UNITS_SET_LITERAL(force, _kN, std::kilo)
SCICPP_CORE_UNITS_SET_LITERAL(force, _MN, std::mega)
SCICPP_CORE_UNITS_SET_LITERAL(force, _GN, std::giga)
SCICPP_CORE_UNITS_SET_LITERAL(force, _TN, std::tera)
SCICPP_CORE_UNITS_SET_LITERAL(force, _PN, std::peta)
SCICPP_CORE_UNITS_SET_LITERAL(force, _EN, std::exa)

SCICPP_CORE_UNITS_SET_LITERAL_RATIO(force, _lbf, 44482216152605, 10000000000000)
SCICPP_CORE_UNITS_SET_LITERAL_RATIO(force, _pdl, 138254954376, 1000000000000)
SCICPP_CORE_UNITS_SET_LITERAL_RATIO(force, _kgf, 980665, 100000)
SCICPP_CORE_UNITS_SET_LITERAL_RATIO(force, _dyn, 1, 100000)

} // namespace literals

// ----------------------------------------------------------------------------
// Power
// ----------------------------------------------------------------------------

SCICPP_CORE_UNITS_DEFINE_PREFIXES_ALIAS(power, watt)

namespace literals {

SCICPP_CORE_UNITS_SET_LITERAL(power, _aW, std::atto)
SCICPP_CORE_UNITS_SET_LITERAL(power, _fW, std::femto)
SCICPP_CORE_UNITS_SET_LITERAL(power, _pW, std::pico)
SCICPP_CORE_UNITS_SET_LITERAL(power, _nW, std::nano)
SCICPP_CORE_UNITS_SET_LITERAL(power, _uW, std::micro)
SCICPP_CORE_UNITS_SET_LITERAL(power, _mW, std::milli)
SCICPP_CORE_UNITS_SET_LITERAL(power, _W, std::ratio<1>)
SCICPP_CORE_UNITS_SET_LITERAL(power, _kW, std::kilo)
SCICPP_CORE_UNITS_SET_LITERAL(power, _MW, std::mega)
SCICPP_CORE_UNITS_SET_LITERAL(power, _GW, std::giga)
SCICPP_CORE_UNITS_SET_LITERAL(power, _TW, std::tera)
SCICPP_CORE_UNITS_SET_LITERAL(power, _PW, std::peta)
SCICPP_CORE_UNITS_SET_LITERAL(power, _EW, std::exa)

} // namespace literals

// ----------------------------------------------------------------------------
// Energy
// ----------------------------------------------------------------------------

SCICPP_CORE_UNITS_DEFINE_PREFIXES_ALIAS(energy, joule)

template <typename T = double>
using watt_hour = energy<T, scale<std::ratio<3600>>>;

template <typename T = double>
using kilowatt_hour = energy<T, scale<std::ratio<3600000>>>;

template <typename T = double>
using megawatt_hour = energy<T, scale<std::ratio<3600000000>>>;

template <typename T = double>
using gigawatt_hour = energy<T, scale<std::ratio<3600000000000>>>;

template <typename T = double>
using terawatt_hour = energy<T, scale<std::ratio<3600000000000000>>>;

template <typename T = double>
using petawatt_hour = energy<T, scale<std::ratio<3600000000000000000>>>;

template <typename T = double>
using calorie = energy<T, scale<std::ratio<4184, 1000>>>;

template <typename T = double>
using kilocalorie = energy<T, scale<std::ratio<4184>>>;

namespace literals {

SCICPP_CORE_UNITS_SET_LITERAL(energy, _aJ, std::atto)
SCICPP_CORE_UNITS_SET_LITERAL(energy, _fJ, std::femto)
SCICPP_CORE_UNITS_SET_LITERAL(energy, _pJ, std::pico)
SCICPP_CORE_UNITS_SET_LITERAL(energy, _nJ, std::nano)
SCICPP_CORE_UNITS_SET_LITERAL(energy, _uJ, std::micro)
SCICPP_CORE_UNITS_SET_LITERAL(energy, _mJ, std::milli)
SCICPP_CORE_UNITS_SET_LITERAL(energy, _J, std::ratio<1>)
SCICPP_CORE_UNITS_SET_LITERAL(energy, _kJ, std::kilo)
SCICPP_CORE_UNITS_SET_LITERAL(energy, _MJ, std::mega)
SCICPP_CORE_UNITS_SET_LITERAL(energy, _GJ, std::giga)
SCICPP_CORE_UNITS_SET_LITERAL(energy, _TJ, std::tera)
SCICPP_CORE_UNITS_SET_LITERAL(energy, _PJ, std::peta)
SCICPP_CORE_UNITS_SET_LITERAL(energy, _EJ, std::exa)

SCICPP_CORE_UNITS_SET_LITERAL_RATIO(energy, _Wh, 3600, 1)
SCICPP_CORE_UNITS_SET_LITERAL_RATIO(energy, _kWh, 3600000, 1)
SCICPP_CORE_UNITS_SET_LITERAL_RATIO(energy, _MWh, 3600000000, 1)
SCICPP_CORE_UNITS_SET_LITERAL_RATIO(energy, _GWh, 3600000000000, 1)
SCICPP_CORE_UNITS_SET_LITERAL_RATIO(energy, _TWh, 3600000000000000, 1)
SCICPP_CORE_UNITS_SET_LITERAL_RATIO(energy, _PWh, 3600000000000000000, 1)

SCICPP_CORE_UNITS_SET_LITERAL_RATIO(energy, _cal, 4184, 1000)
SCICPP_CORE_UNITS_SET_LITERAL_RATIO(energy, _kcal, 4184, 1)

} // namespace literals

// ----------------------------------------------------------------------------
// Pressure
// ----------------------------------------------------------------------------

SCICPP_CORE_UNITS_DEFINE_PREFIXES_ALIAS(pressure, pascal)

template <typename T = double>
using millibar = pressure<T, scale<std::ratio<100>>>;

template <typename T = double>
using bar = pressure<T, scale<std::ratio<100000>>>;

template <typename T = double>
using kilobar = pressure<T, scale<std::ratio<100000000>>>;

template <typename T = double>
using mmHg = pressure<T, scale<std::ratio<101325, 760>>>;

template <typename T = double>
using torr = mmHg<T>;

template <typename T = double>
using psi = pressure<T, scale<std::ratio<689476, 100>>>;

template <typename T = double>
using atm = pressure<T, scale<std::ratio<101325>>>;

namespace literals {

SCICPP_CORE_UNITS_SET_LITERAL(pressure, _aPa, std::atto)
SCICPP_CORE_UNITS_SET_LITERAL(pressure, _fPa, std::femto)
SCICPP_CORE_UNITS_SET_LITERAL(pressure, _pPa, std::pico)
SCICPP_CORE_UNITS_SET_LITERAL(pressure, _nPa, std::nano)
SCICPP_CORE_UNITS_SET_LITERAL(pressure, _uPa, std::micro)
SCICPP_CORE_UNITS_SET_LITERAL(pressure, _mPa, std::milli)
SCICPP_CORE_UNITS_SET_LITERAL(pressure, _Pa, std::ratio<1>)
SCICPP_CORE_UNITS_SET_LITERAL(pressure, _hPa, std::hecto)
SCICPP_CORE_UNITS_SET_LITERAL(pressure, _kPa, std::kilo)
SCICPP_CORE_UNITS_SET_LITERAL(pressure, _MPa, std::mega)
SCICPP_CORE_UNITS_SET_LITERAL(pressure, _GPa, std::giga)
SCICPP_CORE_UNITS_SET_LITERAL(pressure, _TPa, std::tera)
SCICPP_CORE_UNITS_SET_LITERAL(pressure, _PPa, std::peta)
SCICPP_CORE_UNITS_SET_LITERAL(pressure, _EPa, std::exa)

SCICPP_CORE_UNITS_SET_LITERAL_RATIO(pressure, _mbar, 100, 1)
SCICPP_CORE_UNITS_SET_LITERAL_RATIO(pressure, _bar, 100000, 1)
SCICPP_CORE_UNITS_SET_LITERAL_RATIO(pressure, _kbar, 100000000, 1)
SCICPP_CORE_UNITS_SET_LITERAL_RATIO(pressure, _mmHg, 101325, 760)
SCICPP_CORE_UNITS_SET_LITERAL_RATIO(pressure, _torr, 101325, 760)
SCICPP_CORE_UNITS_SET_LITERAL_RATIO(pressure, _psi, 689476, 100)
SCICPP_CORE_UNITS_SET_LITERAL_RATIO(pressure, _atm, 101325, 1)

} // namespace literals

// ----------------------------------------------------------------------------
// Frequency
// ----------------------------------------------------------------------------

SCICPP_CORE_UNITS_DEFINE_PREFIXES_ALIAS(frequency, hertz)

namespace literals {

SCICPP_CORE_UNITS_SET_LITERAL(frequency, _aHz, std::atto)
SCICPP_CORE_UNITS_SET_LITERAL(frequency, _fHz, std::femto)
SCICPP_CORE_UNITS_SET_LITERAL(frequency, _pHz, std::pico)
SCICPP_CORE_UNITS_SET_LITERAL(frequency, _nHz, std::nano)
SCICPP_CORE_UNITS_SET_LITERAL(frequency, _uHz, std::micro)
SCICPP_CORE_UNITS_SET_LITERAL(frequency, _mHz, std::milli)
SCICPP_CORE_UNITS_SET_LITERAL(frequency, _Hz, std::ratio<1>)
SCICPP_CORE_UNITS_SET_LITERAL(frequency, _kHz, std::kilo)
SCICPP_CORE_UNITS_SET_LITERAL(frequency, _MHz, std::mega)
SCICPP_CORE_UNITS_SET_LITERAL(frequency, _GHz, std::giga)
SCICPP_CORE_UNITS_SET_LITERAL(frequency, _THz, std::tera)
SCICPP_CORE_UNITS_SET_LITERAL(frequency, _PHz, std::peta)
SCICPP_CORE_UNITS_SET_LITERAL(frequency, _EHz, std::exa)

} // namespace literals

// ----------------------------------------------------------------------------
// Data rate
// ----------------------------------------------------------------------------

template <typename T = double>
using bit_per_second = data_rate<T>;

template <typename T = double>
using kilobit_per_second = data_rate<T, scale<std::kilo>>;

template <typename T = double>
using megabit_per_second = data_rate<T, scale<std::mega>>;

template <typename T = double>
using gigabit_per_second = data_rate<T, scale<std::giga>>;

template <typename T = double>
using terabit_per_second = data_rate<T, scale<std::tera>>;

template <typename T = double>
using petabit_per_second = data_rate<T, scale<std::peta>>;

template <typename T = double>
using exabit_per_second = data_rate<T, scale<std::exa>>;

namespace literals {

SCICPP_CORE_UNITS_SET_LITERAL(data_rate, _bps, std::ratio<1>)
SCICPP_CORE_UNITS_SET_LITERAL(data_rate, _kbps, std::kilo)
SCICPP_CORE_UNITS_SET_LITERAL(data_rate, _Mbps, std::mega)
SCICPP_CORE_UNITS_SET_LITERAL(data_rate, _Gbps, std::giga)
SCICPP_CORE_UNITS_SET_LITERAL(data_rate, _Tbps, std::tera)
SCICPP_CORE_UNITS_SET_LITERAL(data_rate, _Pbps, std::peta)
SCICPP_CORE_UNITS_SET_LITERAL(data_rate, _Ebps, std::exa)

} // namespace literals

#include "chemical.hpp"
#include "electromagnetism.hpp"
#include "photometry.hpp"
#include "thermal.hpp"

#undef QUANTITY_TRAIT
#undef SCICPP_CORE_UNITS_SET_LITERAL
#undef SCICPP_CORE_UNITS_SET_LITERAL_RATIO
#undef SCICPP_CORE_UNITS_SET_LITERAL_RATIO2
#undef SCICPP_CORE_UNITS_DEFINE_PREFIXES_ALIAS

} // namespace scicpp::units

#endif // SCICPP_CORE_UNITS_UNITS