// SPDX-License-Identifier: MIT
// Copyright (c) 2019-2021 Thomas Vanderbruggen <th.vanderbruggen@gmail.com>

template <typename T, typename Scale = scale<std::ratio<1>>>
using heat = energy<T, Scale>;

QUANTITY_TRAIT(heat)

template <typename T, typename Scale = scale<std::ratio<1>>>
using heat_transfer_rate = power<T, Scale>;

QUANTITY_TRAIT(heat_transfer_rate)

template <typename T, typename Scale = scale<std::ratio<1>>>
using heat_flux = quantity_divide<heat_transfer_rate<T, Scale>, area<T>>;

QUANTITY_TRAIT(heat_flux)

template <typename T, typename Scale = scale<std::ratio<1>>>
using heat_capacity = quantity_divide<heat<T, Scale>, temperature<T>>;

QUANTITY_TRAIT(heat_capacity)

// Thermal resistance = Temperature / Power
template <typename T, typename Scale = scale<std::ratio<1>>>
using thermal_resistance =
    quantity_divide<temperature<T, Scale>, heat_transfer_rate<T>>;

QUANTITY_TRAIT(thermal_resistance)

// Thermal resistivity = Thermal resistance x Length
template <typename T, typename Scale = scale<std::ratio<1>>>
using thermal_resistivity =
    quantity_multiply<thermal_resistance<T, Scale>, length<T>>;

QUANTITY_TRAIT(thermal_resistivity)

// Thermal conductivity = 1 / Thermal resistivity
template <typename T, typename Scale = scale<std::ratio<1>>>
using thermal_conductivity = quantity_invert<thermal_resistivity<T, Scale>>;

QUANTITY_TRAIT(thermal_conductivity)

// Thermal insulance = Temperature x Area
template <typename T, typename Scale = scale<std::ratio<1>>>
using thermal_insulance =
    quantity_multiply<thermal_resistance<T, Scale>, area<T>>;

QUANTITY_TRAIT(thermal_insulance)