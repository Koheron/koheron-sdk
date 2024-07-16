// SPDX-License-Identifier: MIT
// Copyright (c) 2019-2021 Thomas Vanderbruggen <th.vanderbruggen@gmail.com>

// Some utility functions to deal with arrays of tuples or pairs.

#ifndef SCICPP_CORE_TUPLE
#define SCICPP_CORE_TUPLE

#include "scicpp/core/meta.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <tuple>
#include <utility>
#include <vector>

namespace scicpp {

//---------------------------------------------------------------------------------
// get_field
//---------------------------------------------------------------------------------

// ---- By index

template <std::size_t I, typename T, std::size_t N>
auto get_field(const std::array<T, N> &a) {
    static_assert(meta::is_std_tuple_v<T> || meta::is_std_pair_v<T>);
    using EltType = std::tuple_element_t<I, T>;

    auto res = std::array<EltType, N>{};
    std::transform(a.cbegin(), a.cend(), res.begin(), [=](auto t) {
        return std::get<I>(t);
    });
    return res;
}

template <std::size_t I, typename T>
auto get_field(const std::vector<T> &a) {
    static_assert(meta::is_std_tuple_v<T> || meta::is_std_pair_v<T>);
    using EltType = std::tuple_element_t<I, T>;

    auto res = std::vector<EltType>(a.size());
    std::transform(a.cbegin(), a.cend(), res.begin(), [=](auto t) {
        return std::get<I>(t);
    });
    return res;
}

// ---- By type

template <typename EltType, typename T, std::size_t N>
auto get_field(const std::array<T, N> &a) {
    static_assert(meta::is_std_tuple_v<T> || meta::is_std_pair_v<T>);

    auto res = std::array<EltType, N>{};
    std::transform(a.cbegin(), a.cend(), res.begin(), [](auto t) {
        return std::get<EltType>(t);
    });
    return res;
}

template <typename EltType, typename T>
auto get_field(const std::vector<T> &a) {
    static_assert(meta::is_std_tuple_v<T> || meta::is_std_pair_v<T>);

    auto res = std::vector<EltType>(a.size());
    std::transform(a.cbegin(), a.cend(), res.begin(), [](auto t) {
        return std::get<EltType>(t);
    });
    return res;
}

//---------------------------------------------------------------------------------
// unpack: Convert an array of tuples into a tuple of array,
// so that arguments may be unpacked using:
// auto [x, y, z] = unpack(a);
//---------------------------------------------------------------------------------

namespace detail {

template <class Array, class Tuple, std::size_t... I>
auto unpack_impl(const Array &a,
                 Tuple /*unused*/,
                 std::index_sequence<I...> /*unused*/) {
    return std::make_tuple(get_field<I>(a)...);
}

} // namespace detail

template <class Array>
auto unpack(const Array &a) {
    using T = typename Array::value_type;
    static_assert(meta::is_std_tuple_v<T> || meta::is_std_pair_v<T>);

    return detail::unpack_impl(
        a, T{}, std::make_index_sequence<std::tuple_size_v<T>>{});
}

} // namespace scicpp

#endif // SCICPP_CORE_TUPLE