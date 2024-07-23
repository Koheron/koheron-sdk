// SPDX-License-Identifier: MIT
// Copyright (c) 2019-2021 Thomas Vanderbruggen <th.vanderbruggen@gmail.com>

#ifndef SCICPP_CORE_RANGE
#define SCICPP_CORE_RANGE

#include "scicpp/core/functional.hpp"
#include "scicpp/core/numeric.hpp"
#include "scicpp/core/units/quantity.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <numeric>
#include <utility>
#include <vector>

namespace scicpp {

//---------------------------------------------------------------------------------
// empty
//---------------------------------------------------------------------------------

template <typename T>
auto empty() {
    return std::vector<T>(0);
}

//---------------------------------------------------------------------------------
// full
//---------------------------------------------------------------------------------

template <std::size_t N, typename T>
auto full(T fill_value) {
    auto a = std::array<T, N>{};
    a.fill(fill_value);
    return a;
}

template <typename T>
auto full(std::size_t N, T fill_value) {
    return std::vector<T>(N, fill_value);
}

//---------------------------------------------------------------------------------
// zeros
//---------------------------------------------------------------------------------

template <std::size_t N, typename T>
auto zeros() {
    if constexpr (meta::is_complex_v<T>) {
        using Tp = typename T::value_type;
        return full<N>(std::complex(Tp{0}, Tp{0}));
    } else {
        return full<N>(T{0});
    }
}

template <typename T>
auto zeros(std::size_t N) {
    if constexpr (meta::is_complex_v<T>) {
        using Tp = typename T::value_type;
        return full(N, std::complex(Tp{0}, Tp{0}));
    } else {
        return full(N, T{0});
    }
}

//---------------------------------------------------------------------------------
// ones
//---------------------------------------------------------------------------------

template <std::size_t N, typename T>
auto ones() {
    if constexpr (meta::is_complex_v<T>) {
        using Tp = typename T::value_type;
        return full<N>(std::complex(Tp{1}, Tp{0}));
    } else {
        return full<N>(T{1});
    }
}

template <typename T>
auto ones(std::size_t N) {
    if constexpr (meta::is_complex_v<T>) {
        using Tp = typename T::value_type;
        return full(N, std::complex(Tp{1}, Tp{0}));
    } else {
        return full(N, T{1});
    }
}

//---------------------------------------------------------------------------------
// Linspace
//---------------------------------------------------------------------------------

namespace detail {

template <class Array, typename T = typename Array::value_type>
auto linspace_filler(Array &&a, T start, T stop) {
    using namespace scicpp::operators;
    using raw_t = units::representation_t<T>;

    if (a.empty()) {
        return std::move(a);
    }

    if (a.size() == 1) {
        a[0] = start;
        return std::move(a);
    }

    std::iota(a.begin(), a.end(), T{0});
    const auto step = units::value(stop - start) / raw_t(a.size() - 1);
    return start + std::move(a) * step;
}

} // namespace detail

template <std::size_t N, typename T>
auto linspace(T start, T stop) {
    return detail::linspace_filler(std::array<T, N>{}, start, stop);
}

template <typename T>
auto linspace(T start, T stop, std::size_t num) {
    return detail::linspace_filler(std::vector<T>(num), start, stop);
}

//---------------------------------------------------------------------------------
// Logspace
//---------------------------------------------------------------------------------

namespace detail {

template <class Array, typename RepTp, typename T = typename Array::value_type>
auto logspace_filler(Array &&a, RepTp start, RepTp stop, RepTp base) {
    if (a.empty()) {
        return std::move(a);
    }

    if (a.size() == 1) {
        a[0] = T(std::pow(base, start));
        return std::move(a);
    }

    std::iota(a.begin(), a.end(), T{0});
    const auto step = (stop - start) / RepTp(a.size() - 1);

    return map(
        [=](auto x) {
            return T(std::pow(base, std::fma(units::value(x), step, start)));
        },
        std::move(a));
}

} // namespace detail

template <std::size_t N, typename T>
auto logspace(T start, T stop, T base = T{10}) {
    return detail::logspace_filler(std::array<T, N>{}, start, stop, base);
}

template <std::size_t N,
          typename Qty,
          typename RepTp = units::representation_t<Qty>,
          units::enable_if_is_quantity<Qty> = 0>
auto logspace(RepTp start, RepTp stop, RepTp base = RepTp{10}) {
    return detail::logspace_filler(std::array<Qty, N>{}, start, stop, base);
}

template <typename T>
auto logspace(T start, T stop, std::size_t num, T base = T{10}) {
    return detail::logspace_filler(std::vector<T>(num), start, stop, base);
}

template <typename Qty,
          typename RepTp = units::representation_t<Qty>,
          units::enable_if_is_quantity<Qty> = 0>
auto logspace(RepTp start,
              RepTp stop,
              std::size_t num,
              RepTp base = RepTp{10}) {
    return detail::logspace_filler(std::vector<Qty>(num), start, stop, base);
}

//---------------------------------------------------------------------------------
// Arange
//---------------------------------------------------------------------------------

template <typename T>
auto arange(T start, T stop, T step = T{1}) {
    std::size_t num = 0;

    if (((stop > start) && (step < T{0})) ||
        ((stop < start) && (step > T{0}))) {
        num = 0;
    } else {
        num = static_cast<std::size_t>(
            units::value(units::fabs((stop - start) / step)));
    }

    std::vector<T> v(num);
    std::iota(v.begin(), v.end(), T{0});
    return map([=](auto x) { return units::fma(x, units::value(step), start); },
               std::move(v));
}

} // namespace scicpp

#endif // SCICPP_CORE_RANGE
