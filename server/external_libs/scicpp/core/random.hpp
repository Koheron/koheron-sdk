// SPDX-License-Identifier: MIT
// Copyright (c) 2019-2021 Thomas Vanderbruggen <th.vanderbruggen@gmail.com>

#ifndef SCICPP_CORE_RANDOM
#define SCICPP_CORE_RANDOM

#include "scicpp/core/functional.hpp"
#include "scicpp/core/units/quantity.hpp"

#include <array>
#include <cstdint>
#include <random>
#include <type_traits>
#include <utility>
#include <vector>

namespace scicpp::random {

namespace detail {

template <std::uint_fast32_t seed, class Array, class RND>
auto random_number_filler(Array &a, RND distribution) {
    static_assert(
        std::is_same_v<typename Array::value_type, typename RND::result_type>);

    std::mt19937 rng;
    rng.seed(seed);
    return map([&]([[maybe_unused]] auto v) { return distribution(rng); },
               std::move(a));
}

template <typename T>
const auto uniform_dist = std::uniform_real_distribution<T>{0., 1.};

template <typename T>
const auto normal_dist = std::normal_distribution<T>{0., 1.};

} // namespace detail

//---------------------------------------------------------------------------------
// rand
//---------------------------------------------------------------------------------

template <typename T,
          std::size_t N,
          std::uint_fast32_t seed = std::mt19937::default_seed>
auto rand() {
    std::array<T, N> res{};
    return detail::random_number_filler<seed>(res, detail::uniform_dist<T>);
}

template <typename T>
T rand() {
    return rand<T, 1, std::mt19937::default_seed>()[0];
}

template <typename T, std::uint_fast32_t seed = std::mt19937::default_seed>
auto rand(std::size_t N) {
    std::vector<T> res(N);
    return detail::random_number_filler<seed>(res, detail::uniform_dist<T>);
}

//---------------------------------------------------------------------------------
// randn
//---------------------------------------------------------------------------------

template <typename T,
          std::size_t N,
          std::uint_fast32_t seed = std::mt19937::default_seed>
auto randn() {
    std::array<T, N> res{};
    return detail::random_number_filler<seed>(res, detail::normal_dist<T>);
}

template <typename T>
T randn() {
    return randn<T, 1, std::mt19937::default_seed>()[0];
}

template <typename T, std::uint_fast32_t seed = std::mt19937::default_seed>
auto randn(std::size_t N) {
    std::vector<T> res(N);
    return detail::random_number_filler<seed>(res, detail::normal_dist<T>);
}

//---------------------------------------------------------------------------------
// normal
// Draw random samples from a normal (Gaussian) distribution.
// https://numpy.org/doc/stable/reference/random/generated/numpy.random.normal.html
//---------------------------------------------------------------------------------

template <std::size_t N, typename T>
auto normal(T mu, T sigma) {
    using namespace operators;
    using rep_t = units::representation_t<T>;

    return mu + sigma * randn<rep_t, N>();
}

template <typename T>
auto normal(T mu, T sigma, std::size_t N) {
    using namespace operators;
    using rep_t = units::representation_t<T>;

    return mu + sigma * random::randn<rep_t>(N);
}

} // namespace scicpp::random

#endif // SCICPP_CORE_RANDOM