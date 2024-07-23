// SPDX-License-Identifier: MIT
// Copyright (c) 2019-2021 Thomas Vanderbruggen <th.vanderbruggen@gmail.com>

// Chemical potential = Energy / Amount of substance
template <typename T, typename Scale = scale<std::ratio<1>>>
using chemical_potential =
    quantity_divide<energy<T, Scale>, amount_of_substance<T>>;

QUANTITY_TRAIT(chemical_potential)

// Molarity = Amount of substance / Volume
template <typename T, typename Scale = scale<std::ratio<1>>>
using molarity = quantity_divide<amount_of_substance<T, Scale>, volume<T>>;

QUANTITY_TRAIT(molarity)

// Molality = Amount of substance / Mass
template <typename T, typename Scale = scale<std::ratio<1>>>
using molality = quantity_divide<amount_of_substance<T, Scale>, mass<T>>;

QUANTITY_TRAIT(molality)

// Catalytic activity = Amount of substance / Time
template <typename T, typename Scale = scale<std::ratio<1>>>
using catalytic_activity =
    quantity_divide<amount_of_substance<T, Scale>, time<T>>;

QUANTITY_TRAIT(catalytic_activity)

// ----------------------------------------------------------------------------
// Catalytic activity
// ----------------------------------------------------------------------------

SCICPP_CORE_UNITS_DEFINE_PREFIXES_ALIAS(catalytic_activity, katal)

template <typename T = double>
using enzyme_unit =
    catalytic_activity<T, scale<std::ratio<1667, 100000000000>>>;

namespace literals {

SCICPP_CORE_UNITS_SET_LITERAL(catalytic_activity, _akat, std::atto)
SCICPP_CORE_UNITS_SET_LITERAL(catalytic_activity, _fkat, std::femto)
SCICPP_CORE_UNITS_SET_LITERAL(catalytic_activity, _pkat, std::pico)
SCICPP_CORE_UNITS_SET_LITERAL(catalytic_activity, _nkat, std::nano)
SCICPP_CORE_UNITS_SET_LITERAL(catalytic_activity, _ukat, std::micro)
SCICPP_CORE_UNITS_SET_LITERAL(catalytic_activity, _mkat, std::milli)
SCICPP_CORE_UNITS_SET_LITERAL(catalytic_activity, _kat, std::ratio<1>)
SCICPP_CORE_UNITS_SET_LITERAL(catalytic_activity, _kkat, std::kilo)
SCICPP_CORE_UNITS_SET_LITERAL(catalytic_activity, _Mkat, std::mega)
SCICPP_CORE_UNITS_SET_LITERAL(catalytic_activity, _Gkat, std::giga)
SCICPP_CORE_UNITS_SET_LITERAL(catalytic_activity, _Tkat, std::tera)
SCICPP_CORE_UNITS_SET_LITERAL(catalytic_activity, _Pkat, std::peta)
SCICPP_CORE_UNITS_SET_LITERAL(catalytic_activity, _Ekat, std::exa)

} // namespace literals