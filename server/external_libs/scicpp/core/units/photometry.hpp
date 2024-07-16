// SPDX-License-Identifier: MIT
// Copyright (c) 2019-2021 Thomas Vanderbruggen <th.vanderbruggen@gmail.com>

// https://en.wikipedia.org/wiki/Photometry_(optics)

// Luminous flux = Luminous intensity x Solid angle
template <typename T, typename Scale = scale<std::ratio<1>>>
using luminous_flux =
    quantity_multiply<luminous_intensity<T, Scale>, solid_angle<T>>;

QUANTITY_TRAIT(luminous_flux)

// Luminous energy = Luminous flux x Time
template <typename T, typename Scale = scale<std::ratio<1>>>
using luminous_energy = quantity_multiply<luminous_flux<T, Scale>, time<T>>;

QUANTITY_TRAIT(luminous_energy)

// Luminance = Luminous intensity / Area
template <typename T, typename Scale = scale<std::ratio<1>>>
using luminance = quantity_divide<luminous_intensity<T, Scale>, area<T>>;

QUANTITY_TRAIT(luminance)

// Illuminance = Luminous flux / Area
template <typename T, typename Scale = scale<std::ratio<1>>>
using illuminance = quantity_divide<luminous_flux<T, Scale>, area<T>>;

QUANTITY_TRAIT(illuminance)

// Luminous exposure = Illuminance x Time
template <typename T, typename Scale = scale<std::ratio<1>>>
using luminous_exposure = quantity_multiply<illuminance<T, Scale>, time<T>>;

QUANTITY_TRAIT(luminous_exposure)

// Luminous energy density = Luminous energy / Volume
template <typename T, typename Scale = scale<std::ratio<1>>>
using luminous_energy_density =
    quantity_divide<luminous_energy<T, Scale>, volume<T>>;

QUANTITY_TRAIT(luminous_energy_density)

// Luminous efficacy = Luminous flux / Power
template <typename T, typename Scale = scale<std::ratio<1>>>
using luminous_efficacy = quantity_divide<luminous_flux<T, Scale>, power<T>>;

QUANTITY_TRAIT(luminous_efficacy)

// ----------------------------------------------------------------------------
// Luminous flux
// ----------------------------------------------------------------------------

SCICPP_CORE_UNITS_DEFINE_PREFIXES_ALIAS(luminous_flux, lumen)

namespace literals {

SCICPP_CORE_UNITS_SET_LITERAL(luminous_flux, _alm, std::atto)
SCICPP_CORE_UNITS_SET_LITERAL(luminous_flux, _flm, std::pico)
SCICPP_CORE_UNITS_SET_LITERAL(luminous_flux, _nlm, std::nano)
SCICPP_CORE_UNITS_SET_LITERAL(luminous_flux, _ulm, std::micro)
SCICPP_CORE_UNITS_SET_LITERAL(luminous_flux, _mlm, std::milli)
SCICPP_CORE_UNITS_SET_LITERAL(luminous_flux, _lm, std::ratio<1>)
SCICPP_CORE_UNITS_SET_LITERAL(luminous_flux, _klm, std::kilo)
SCICPP_CORE_UNITS_SET_LITERAL(luminous_flux, _Mlm, std::mega)
SCICPP_CORE_UNITS_SET_LITERAL(luminous_flux, _Glm, std::giga)
SCICPP_CORE_UNITS_SET_LITERAL(luminous_flux, _Tlm, std::tera)
SCICPP_CORE_UNITS_SET_LITERAL(luminous_flux, _Plm, std::peta)
SCICPP_CORE_UNITS_SET_LITERAL(luminous_flux, _Elm, std::exa)

} // namespace literals

// ----------------------------------------------------------------------------
// Illuminance
// ----------------------------------------------------------------------------

SCICPP_CORE_UNITS_DEFINE_PREFIXES_ALIAS(illuminance, lux)

namespace literals {

SCICPP_CORE_UNITS_SET_LITERAL(illuminance, _alx, std::atto)
SCICPP_CORE_UNITS_SET_LITERAL(illuminance, _flx, std::pico)
SCICPP_CORE_UNITS_SET_LITERAL(illuminance, _nlx, std::nano)
SCICPP_CORE_UNITS_SET_LITERAL(illuminance, _ulx, std::micro)
SCICPP_CORE_UNITS_SET_LITERAL(illuminance, _mlx, std::milli)
SCICPP_CORE_UNITS_SET_LITERAL(illuminance, _lx, std::ratio<1>)
SCICPP_CORE_UNITS_SET_LITERAL(illuminance, _klx, std::kilo)
SCICPP_CORE_UNITS_SET_LITERAL(illuminance, _Mlx, std::mega)
SCICPP_CORE_UNITS_SET_LITERAL(illuminance, _Glx, std::giga)
SCICPP_CORE_UNITS_SET_LITERAL(illuminance, _Tlx, std::tera)
SCICPP_CORE_UNITS_SET_LITERAL(illuminance, _Plx, std::peta)
SCICPP_CORE_UNITS_SET_LITERAL(illuminance, _Elx, std::exa)

} // namespace literals