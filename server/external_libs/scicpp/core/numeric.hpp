// SPDX-License-Identifier: MIT
// Copyright (c) 2019-2021 Thomas Vanderbruggen <th.vanderbruggen@gmail.com>

#ifndef SCICPP_CORE_NUMERIC
#define SCICPP_CORE_NUMERIC

#include "scicpp/core/functional.hpp"
#include "scicpp/core/macros.hpp"
#include "scicpp/core/meta.hpp"
#include "scicpp/core/units/quantity.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <concepts>
#include <cstddef>
#include <functional>
#include <iterator>
#include <numeric>
#include <ranges>
#include <span>
#include <type_traits>
#include <vector>

namespace scicpp {

//---------------------------------------------------------------------------------
// sum
//---------------------------------------------------------------------------------

template <std::input_iterator It, std::sentinel_for<It> S, class Predicate>
[[nodiscard]] constexpr auto sum(It first, S last, Predicate &&pred) {
    return filter_reduce_associative(
        first, last, std::plus<>{}, std::forward<Predicate>(pred));
}

template <class InputIt>
[[nodiscard]] constexpr auto sum(InputIt first, InputIt last) {
    return std::get<0>(sum(first, last, filters::all));
}

template <std::ranges::input_range R, class Predicate>
[[nodiscard]] constexpr auto sum(R &&r, Predicate &&filter) {
    return sum(std::cbegin(r), std::cend(r), std::forward<Predicate>(filter));
}

template <class Range>
[[nodiscard]] constexpr auto sum(const Range &r) {
    return std::get<0>(sum(r, filters::all));
}

template <class Range>
[[nodiscard]] auto nansum(const Range &r) {
    return sum(r, filters::not_nan);
}

//---------------------------------------------------------------------------------
// prod
//---------------------------------------------------------------------------------

template <std::input_iterator It, std::sentinel_for<It> S, class Predicate>
[[nodiscard]] constexpr auto prod(It first, S last, Predicate &&filter) {
    using T = std::iter_value_t<It>;
    return filter_reduce_associative(first,
                                     last,
                                     std::multiplies<>{},
                                     std::forward<Predicate>(filter),
                                     T{1});
}

template <std::input_iterator It, std::sentinel_for<It> S>
[[nodiscard]] constexpr auto prod(It first, S last) {
    return std::get<0>(prod(first, last, filters::all));
}

template <std::ranges::input_range R, class Predicate>
[[nodiscard]] constexpr auto prod(R &&r, Predicate &&filter) {
    return prod(std::cbegin(r), std::cend(r), std::forward<Predicate>(filter));
}

template <std::ranges::input_range R>
[[nodiscard]] constexpr auto prod(R &&r) {
    return std::get<0>(prod(std::forward<R>(r), filters::all));
}

template <std::ranges::input_range R>
[[nodiscard]] constexpr auto nanprod(R &&r) {
    return prod(std::forward<R>(r), filters::not_nan);
}

//---------------------------------------------------------------------------------
// cumsum
//---------------------------------------------------------------------------------

template <class Array>
[[nodiscard]] constexpr auto cumsum(Array &&a) {
    std::partial_sum(std::cbegin(a), std::cend(a), std::begin(a));
    return std::move(a);
}

template <class Array>
[[nodiscard]] constexpr auto cumsum(const Array &a) {
    return cumsum(Array(a));
}

template <class T, std::size_t Extent>
[[nodiscard]] constexpr auto cumsum(std::span<T, Extent> s) {
    std::partial_sum(s.begin(), s.end(), s.begin());
    return s;
}

template <class T, std::size_t Extent>
[[nodiscard]] auto cumsum(std::span<const T, Extent> s) {
    std::vector<T> out(s.begin(), s.end());
    std::partial_sum(out.begin(), out.end(), out.begin());
    return out;
}

template <std::ranges::input_range R>
[[nodiscard]] auto nancumsum(R &&r) {
    return cumacc(std::forward<R>(r), std::plus<>{}, filters::not_nan);
}

//---------------------------------------------------------------------------------
// cumprod
//---------------------------------------------------------------------------------

template <class Array>
[[nodiscard]] constexpr auto cumprod(Array &&a) {
    std::partial_sum(
        std::cbegin(a), std::cend(a), std::begin(a), std::multiplies<>{});
    return std::move(a);
}

template <class Array>
[[nodiscard]] constexpr auto cumprod(const Array &a) {
    return cumprod(Array(a));
}

template <class T, std::size_t Extent>
[[nodiscard]] constexpr auto cumprod(std::span<T, Extent> s) {
    std::partial_sum(s.begin(), s.end(), s.begin(), std::multiplies<>{});
    return s;
}

template <class T, std::size_t Extent>
[[nodiscard]] auto cumprod(std::span<const T, Extent> s) {
    std::vector<T> out(s.begin(), s.end());
    std::partial_sum(out.begin(), out.end(), out.begin(), std::multiplies<>{});
    return out;
}

template <std::ranges::input_range R>
[[nodiscard]] auto nancumprod(R &&r) {
    return cumacc(std::forward<R>(r), std::multiplies<>{}, filters::not_nan);
}

//---------------------------------------------------------------------------------
// trapz
//---------------------------------------------------------------------------------

template <std::ranges::random_access_range R, class Dx>
[[nodiscard]] constexpr auto trapz(R &&r, const Dx &dx) {
    using T1 = std::remove_cvref_t<std::ranges::range_value_t<R>>;
    using ret_t = decltype(std::declval<T1>() * std::declval<Dx>());
    using raw_t = units::representation_t<ret_t>;
    using dx_t = std::conditional_t<units::is_quantity_v<Dx>, Dx, raw_t>;

    const auto n = std::ranges::size(r);

    if (n < 2) {
        return ret_t(raw_t{0});
    }

    const auto first = std::ranges::begin(r);
    const auto last = std::ranges::end(r);

    const auto &f0 = *first;
    const auto &fn_1 = *std::ranges::prev(last);

    const auto interior_first = std::ranges::next(first);
    const auto interior_last = std::ranges::prev(last);
    const auto interior = std::ranges::subrange(interior_first, interior_last);

    const auto s = sum(interior);
    return raw_t{0.5} * dx_t(dx) * (f0 + raw_t{2} * s + fn_1);
}

template <std::input_iterator It, std::sentinel_for<It> S, class Dx>
[[nodiscard]] constexpr auto trapz(It first, S last, const Dx &dx) {
    return trapz(std::ranges::subrange(first, last), dx);
}

//---------------------------------------------------------------------------------
// diff
//---------------------------------------------------------------------------------

namespace detail {

template <typename T, std::size_t N>
constexpr auto diff_once(const std::array<T, N> &a) {
    if constexpr (N <= 1) {
        return std::array<T, 0>{};
    } else {
        std::array<T, N - 1> res{};
        std::adjacent_difference(a.cbegin() + 1, a.cend(), res.begin());
        res[0] = a[1] - a[0];
        return res;
    }
}

template <typename T>
void diff_once(std::vector<T> &res) {
    scicpp_require(!res.empty());
    std::adjacent_difference(res.begin(), res.end(), res.begin());
    res.erase(res.begin());
}

} // namespace detail

template <int n, typename T, std::size_t N>
constexpr auto diff(const std::array<T, N> &a) {
    static_assert(n >= 0);

    if constexpr (n == 0) {
        return std::array{a};
    } else {
        return diff<n - 1>(detail::diff_once(a));
    }
}

template <typename T, std::size_t N>
constexpr auto diff(const std::array<T, N> &a) {
    return diff<1>(a);
}

template <typename T>
auto diff(std::vector<T> &&a, int n = 1) {
    scicpp_require(n >= 0);

    while (n-- && !a.empty()) {
        detail::diff_once(a);
    }

    return std::move(a);
}

template <typename T>
auto diff(const std::vector<T> &a, int n = 1) {
    return diff(std::vector<T>(a), n);
}

//---------------------------------------------------------------------------------
// inner, dot, vdot
//---------------------------------------------------------------------------------

template <std::input_iterator I1,
          std::sentinel_for<I1> S1,
          std::input_iterator I2,
          std::sentinel_for<I2> S2,
          class ProductOp>
[[nodiscard]] constexpr scicpp_pure auto
inner(I1 first1, S1 last1, I2 first2, S2 last2, ProductOp op) {
    using T =
        std::invoke_result_t<ProductOp,
                             typename std::iterator_traits<I1>::value_type,
                             typename std::iterator_traits<I2>::value_type>;
    scicpp_require(std::distance(first1, last1) ==
                   std::distance(first2, last2));

    return pairwise_accumulate<64>(
        first1,
        last1,
        first2,
        last2,
        [&](auto f1, auto l1, auto f2, [[maybe_unused]] auto l2) scicpp_pure {
            auto res = T{0};

            for (; f1 != l1; ++f1, ++f2) {
                res += op(*f1, *f2);
            }

            return res;
        },
        std::plus<>());
}

template <std::input_iterator I1,
          std::sentinel_for<I1> S1,
          std::input_iterator I2,
          std::sentinel_for<I2> S2>
[[nodiscard]] constexpr auto inner(I1 first1, S1 last1, I2 first2, S2 last2) {
    return inner(first1, last1, first2, last2, std::multiplies<>());
}

template <std::ranges::input_range R1, std::ranges::input_range R2>
[[nodiscard]] constexpr auto inner(R1 &&r1, R2 &&r2) {
    return inner(std::ranges::begin(r1),
                 std::ranges::end(r1),
                 std::ranges::begin(r2),
                 std::ranges::end(r2));
}

// inner and dot are the same for 1D arrays
template <std::ranges::input_range R1, std::ranges::input_range R2>
[[nodiscard]] constexpr auto dot(R1 &&r1, R2 &&r2) {
    return inner(std::forward<R1>(r1), std::forward<R2>(r2));
}

template <std::input_iterator I1,
          std::sentinel_for<I1> S1,
          std::input_iterator I2,
          std::sentinel_for<I2> S2>
[[nodiscard]] constexpr auto vdot(I1 first1, S1 last1, I2 first2, S2 last2) {
    return inner(first1, last1, first2, last2, [](auto x1, auto x2) {
        if constexpr (meta::is_complex_v<decltype(x1)>) {
            return std::conj(x1) * x2;
        } else {
            return x1 * x2;
        }
    });
}

template <std::ranges::input_range R1, std::ranges::input_range R2>
[[nodiscard]] constexpr auto vdot(R1 &&r1, R2 &&r2) {
    return vdot(std::ranges::begin(r1),
                std::ranges::end(r1),
                std::ranges::begin(r2),
                std::ranges::end(r2));
}

//---------------------------------------------------------------------------------
// Arithmetic operators
//
// Implements element wise arithmetic operations for std::array and std::vector.
//---------------------------------------------------------------------------------

namespace operators {

namespace detail {

// We define the operator for iterable types which are not
// Eigen::Matrix or Eigen::Array.

template <class T>
constexpr bool is_operator_iterable_v =
    meta::Iterable<T> && !meta::is_eigen_container_v<T>;

template <class T>
using enable_if_operator_iterable =
    std::enable_if_t<detail::is_operator_iterable_v<T>, int>;

template <class T>
using enable_if_scalar =
    std::enable_if_t<std::is_arithmetic_v<T> || meta::is_complex_v<T> ||
                         units::is_quantity_v<T>,
                     int>;

} // namespace detail

// negate

template <class Array, detail::enable_if_operator_iterable<Array> = 0>
constexpr auto operator-(Array &&a) {
    return map(std::negate<>(), std::forward<Array>(a));
}

// logical not

template <class Array, detail::enable_if_operator_iterable<Array> = 0>
constexpr auto operator!(Array &&a) {
    return map(std::logical_not<>(), std::forward<Array>(a));
}

// scalar compare
template <class Array,
          typename T = Array::value_type,
          detail::enable_if_operator_iterable<Array> = 0,
          detail::enable_if_scalar<T> = 0>
constexpr auto operator==(Array &&a, T scalar) {
    return map([=](auto v) { return v == scalar; }, std::forward<Array>(a));
}

template <class Array,
          typename T = Array::value_type,
          detail::enable_if_operator_iterable<Array> = 0,
          detail::enable_if_scalar<T> = 0>
constexpr auto operator==(T scalar, Array &&a) {
    return a == scalar;
}

template <class Array,
          typename T = Array::value_type,
          detail::enable_if_operator_iterable<Array> = 0,
          detail::enable_if_scalar<T> = 0>
constexpr auto operator!=(Array &&a, T scalar) {
    return !(a == scalar);
}

template <class Array,
          typename T = Array::value_type,
          detail::enable_if_operator_iterable<Array> = 0,
          detail::enable_if_scalar<T> = 0>
constexpr auto operator!=(T scalar, Array &&a) {
    return !(a == scalar);
}

template <class Array,
          typename T = Array::value_type,
          detail::enable_if_operator_iterable<Array> = 0,
          detail::enable_if_scalar<T> = 0>
constexpr auto operator<(Array &&a, T scalar) {
    return map([=](auto v) { return v < scalar; }, std::forward<Array>(a));
}

template <class Array,
          typename T = Array::value_type,
          detail::enable_if_operator_iterable<Array> = 0,
          detail::enable_if_scalar<T> = 0>
constexpr auto operator>=(Array &&a, T scalar) {
    return !(a < scalar);
}

template <class Array,
          typename T = Array::value_type,
          detail::enable_if_operator_iterable<Array> = 0,
          detail::enable_if_scalar<T> = 0>
constexpr auto operator<(T scalar, Array &&a) {
    return map([=](auto v) { return scalar < v; }, std::forward<Array>(a));
}

template <class Array,
          typename T = Array::value_type,
          detail::enable_if_operator_iterable<Array> = 0,
          detail::enable_if_scalar<T> = 0>
constexpr auto operator>=(T scalar, Array &&a) {
    return !(scalar < a);
}

template <class Array,
          typename T = Array::value_type,
          detail::enable_if_operator_iterable<Array> = 0,
          detail::enable_if_scalar<T> = 0>
constexpr auto operator<=(Array &&a, T scalar) {
    return !(scalar < a);
}

template <class Array,
          typename T = Array::value_type,
          detail::enable_if_operator_iterable<Array> = 0,
          detail::enable_if_scalar<T> = 0>
constexpr auto operator<=(T scalar, Array &&a) {
    return !(a < scalar);
}

template <class Array,
          typename T = Array::value_type,
          detail::enable_if_operator_iterable<Array> = 0,
          detail::enable_if_scalar<T> = 0>
constexpr auto operator>(Array &&a, T scalar) {
    return !(a <= scalar);
}

template <class Array,
          typename T = Array::value_type,
          detail::enable_if_operator_iterable<Array> = 0,
          detail::enable_if_scalar<T> = 0>
constexpr auto operator>(T scalar, Array &&a) {
    return !(scalar <= a);
}

// scalar multiply
template <class Array,
          typename T = Array::value_type,
          detail::enable_if_operator_iterable<Array> = 0,
          detail::enable_if_scalar<T> = 0>
constexpr auto operator*(Array &&a, T scalar) {
    return map([=](auto v) { return scalar * v; }, std::forward<Array>(a));
}

template <class Array,
          typename T = Array::value_type,
          detail::enable_if_operator_iterable<Array> = 0,
          detail::enable_if_scalar<T> = 0>
constexpr auto operator*(T scalar, Array &&a) {
    return map([=](auto v) { return scalar * v; }, std::forward<Array>(a));
}

// scalar add

template <class Array,
          typename T = Array::value_type,
          detail::enable_if_operator_iterable<Array> = 0,
          detail::enable_if_scalar<T> = 0>
constexpr auto operator+(Array &&a, T scalar) {
    return map([=](auto v) { return scalar + v; }, std::forward<Array>(a));
}

template <class Array,
          typename T = Array::value_type,
          detail::enable_if_operator_iterable<Array> = 0,
          detail::enable_if_scalar<T> = 0>
constexpr auto operator+(T scalar, Array &&a) {
    return map([=](auto v) { return scalar + v; }, std::forward<Array>(a));
}

// scalar substract

template <class Array,
          typename T = Array::value_type,
          detail::enable_if_operator_iterable<Array> = 0,
          detail::enable_if_scalar<T> = 0>
constexpr auto operator-(Array &&a, T scalar) {
    return map([=](auto v) { return v - scalar; }, std::forward<Array>(a));
}

template <class Array,
          typename T = Array::value_type,
          detail::enable_if_operator_iterable<Array> = 0,
          detail::enable_if_scalar<T> = 0>
constexpr auto operator-(T scalar, Array &&a) {
    return map([=](auto v) { return scalar - v; }, std::forward<Array>(a));
}

// scalar divide

template <class Array,
          typename T = Array::value_type,
          detail::enable_if_operator_iterable<Array> = 0,
          detail::enable_if_scalar<T> = 0>
constexpr auto operator/(Array &&a, T scalar) {
    return map([=](auto v) { return v / scalar; }, std::forward<Array>(a));
}

template <class Array,
          typename T = Array::value_type,
          detail::enable_if_operator_iterable<Array> = 0,
          detail::enable_if_scalar<T> = 0>
constexpr auto operator/(T scalar, Array &&a) {
    return map([=](auto v) { return scalar / v; }, std::forward<Array>(a));
}

// scalar modulus

namespace detail {

template <typename T>
constexpr auto modulus(T x, T y) {
    using raw_t = units::representation_t<T>;

    if constexpr (std::is_floating_point_v<raw_t>) {
        return T(std::fmod(units::value(x), units::value(y)));
    } else {
        return T(units::value(x) % units::value(y));
    }
}

} // namespace detail

template <class Array,
          typename T = Array::value_type,
          detail::enable_if_operator_iterable<Array> = 0,
          detail::enable_if_scalar<T> = 0>
constexpr auto operator%(Array &&a, T scalar) {
    return map([=](auto v) { return detail::modulus(v, scalar); },
               std::forward<Array>(a));
}

template <class Array,
          typename T = typename Array::value_type,
          detail::enable_if_operator_iterable<Array> = 0,
          detail::enable_if_scalar<T> = 0>
constexpr auto operator%(T scalar, Array &&a) {
    return map([=](auto v) { return detail::modulus(scalar, v); },
               std::forward<Array>(a));
}

template <class ArrayLhs,
          class ArrayRhs,
          detail::enable_if_operator_iterable<ArrayLhs> = 0,
          detail::enable_if_operator_iterable<ArrayRhs> = 0>
constexpr auto operator*(ArrayLhs &&a, ArrayRhs &&b) {
    return map(std::multiplies<>(),
               std::forward<ArrayLhs>(a),
               std::forward<ArrayRhs>(b));
}

template <class ArrayLhs,
          class ArrayRhs,
          detail::enable_if_operator_iterable<ArrayLhs> = 0,
          detail::enable_if_operator_iterable<ArrayRhs> = 0>
constexpr auto operator&&(ArrayLhs &&a, ArrayRhs &&b) {
    return map(std::logical_and<>(),
               std::forward<ArrayLhs>(a),
               std::forward<ArrayRhs>(b));
}

template <class ArrayLhs,
          class ArrayRhs,
          detail::enable_if_operator_iterable<ArrayLhs> = 0,
          detail::enable_if_operator_iterable<ArrayRhs> = 0>
constexpr auto operator||(ArrayLhs &&a, ArrayRhs &&b) {
    return map(std::logical_or<>(),
               std::forward<ArrayLhs>(a),
               std::forward<ArrayRhs>(b));
}

// Sum of 3 vectors:
// https://godbolt.org/z/zsZB29
// No copy, similar assembly code than raw loop:
// https://godbolt.org/z/ptIXJ4
// Except 4 calls to new with raw copy, only one with operator+.

template <class ArrayLhs,
          class ArrayRhs,
          detail::enable_if_operator_iterable<ArrayLhs> = 0,
          detail::enable_if_operator_iterable<ArrayRhs> = 0>
constexpr auto operator+(ArrayLhs &&a, ArrayRhs &&b) {
    return map(
        std::plus<>(), std::forward<ArrayLhs>(a), std::forward<ArrayRhs>(b));
}

template <class ArrayLhs,
          class ArrayRhs,
          detail::enable_if_operator_iterable<ArrayLhs> = 0,
          detail::enable_if_operator_iterable<ArrayRhs> = 0>
constexpr auto operator-(ArrayLhs &&a, ArrayRhs &&b) {
    return map(
        std::minus<>(), std::forward<ArrayLhs>(a), std::forward<ArrayRhs>(b));
}

template <class ArrayLhs,
          class ArrayRhs,
          detail::enable_if_operator_iterable<ArrayLhs> = 0,
          detail::enable_if_operator_iterable<ArrayRhs> = 0>
constexpr auto operator/(ArrayLhs &&a, ArrayRhs &&b) {
    return map(
        std::divides<>(), std::forward<ArrayLhs>(a), std::forward<ArrayRhs>(b));
}

template <class ArrayLhs,
          class ArrayRhs,
          detail::enable_if_operator_iterable<ArrayLhs> = 0,
          detail::enable_if_operator_iterable<ArrayRhs> = 0>
constexpr auto operator%(ArrayLhs &&a, ArrayRhs &&b) {
    return map([](auto u, auto v) { return detail::modulus(u, v); },
               std::forward<ArrayLhs>(a),
               std::forward<ArrayRhs>(b));
}

} // namespace operators

//---------------------------------------------------------------------------------
// Comparison
//
// In the C++ standard comparison operators are used for lexicographical order.
// So we implement the Numpy comparison function, but not the related operators.
//---------------------------------------------------------------------------------

template <meta::Iterable ArrayLhs, meta::Iterable ArrayRhs>
constexpr auto equal(ArrayLhs &&a, ArrayRhs &&b) {
    return map([](auto u, auto v) { return u == v; },
               std::forward<ArrayLhs>(a),
               std::forward<ArrayRhs>(b));
}

template <meta::Iterable ArrayLhs, meta::Iterable ArrayRhs>
constexpr auto not_equal(ArrayLhs &&a, ArrayRhs &&b) {
    using namespace operators;
    return !equal(std::forward<ArrayLhs>(a), std::forward<ArrayRhs>(b));
}

template <meta::Iterable ArrayLhs, meta::Iterable ArrayRhs>
constexpr auto less(ArrayLhs &&a, ArrayRhs &&b) {
    return map([](auto u, auto v) { return u < v; },
               std::forward<ArrayLhs>(a),
               std::forward<ArrayRhs>(b));
}

template <meta::Iterable ArrayLhs, meta::Iterable ArrayRhs>
constexpr auto less_equal(ArrayLhs &&a, ArrayRhs &&b) {
    using namespace operators;
    return !less(std::forward<ArrayLhs>(b), std::forward<ArrayRhs>(a));
}

template <meta::Iterable ArrayLhs, meta::Iterable ArrayRhs>
constexpr auto greater_equal(ArrayLhs &&a, ArrayRhs &&b) {
    using namespace operators;
    return !less(std::forward<ArrayLhs>(a), std::forward<ArrayRhs>(b));
}

template <meta::Iterable ArrayLhs, meta::Iterable ArrayRhs>
constexpr auto greater(ArrayLhs &&a, ArrayRhs &&b) {
    return less(std::forward<ArrayLhs>(b), std::forward<ArrayRhs>(a));
}

//---------------------------------------------------------------------------------
// Masking
//---------------------------------------------------------------------------------

template <class Array, class Mask>
auto mask(const Array &a, const Mask &m) {
    scicpp_require(a.size() == m.size());
    static_assert(std::is_integral_v<typename Mask::value_type>);

    auto res = std::vector<typename Array::value_type>(0);
    res.reserve(a.size());

    for (std::size_t i = 0; i < a.size(); ++i) {
        if (m[i]) {
            res.push_back(a[i]);
        }
    }

    return res;
}

template <typename T, class Mask>
auto mask(std::vector<T> &&a, const Mask &m) {
    scicpp_require(a.size() == m.size());
    static_assert(std::is_integral_v<typename Mask::value_type>);

    std::size_t idx = 0;

    for (std::size_t i = 0; i < a.size(); ++i) {
        if (m[i]) {
            a[idx] = a[i];
            ++idx;
        }
    }

    a.resize(idx);
    return std::move(a);
}

// Mask a std::vector inplace
// Not possible for std::array since return size is not known at compile time.

template <typename T, class Mask>
void mask_array(std::vector<T> &a, const Mask &m) {
    a = mask(std::move(a), m);
}

//---------------------------------------------------------------------------------
// argmin, argmax
//---------------------------------------------------------------------------------

namespace detail {

template <class InputIt, class Predicate, class Comparator>
constexpr scicpp_pure auto
argcmp(InputIt first, InputIt last, Predicate filter, Comparator compare) {
    using IteratorType = std::iterator_traits<InputIt>::value_type;
    using IdxTp = std::iterator_traits<InputIt>::difference_type;

    static_assert(meta::is_predicate<Predicate, IteratorType>);
    static_assert(meta::is_predicate<Comparator, IteratorType, IteratorType>);
    scicpp_require(std::distance(first, last) > 0);

    auto it = first;

    while (!filter(*it)) {
        scicpp_require(it != last && "No valid value found");
        ++it;
    }

    if (std::distance(it, last) == 1) {
        return std::distance(first, it);
    }

    IdxTp idx = 0;
    auto max = *it;
    ++it;

    for (; it != last; ++it) {
        if (filter(*it) && compare(*it, max)) {
            max = *it;
            idx = std::distance(first, it);
        }
    }

    return idx;
}

} // namespace detail

// argmax

template <class InputIt, class Predicate>
constexpr scicpp_pure auto
argmax(InputIt first, InputIt last, Predicate filter) {
    return detail::argcmp(
        first, last, filter, [](auto u, auto v) { return u > v; });
}

template <class Array, class Predicate>
constexpr scicpp_pure auto argmax(const Array &a, Predicate filter) {
    return argmax(a.cbegin(), a.cend(), filter);
}

template <class Array>
constexpr scicpp_pure auto argmax(const Array &a) {
    return argmax(a.cbegin(), a.cend(), filters::all);
}

template <class Array>
constexpr scicpp_pure auto nanargmax(const Array &a) {
    return argmax(a.cbegin(), a.cend(), filters::not_nan);
}

// argmin

template <class InputIt, class Predicate>
constexpr scicpp_pure auto
argmin(InputIt first, InputIt last, Predicate filter) {
    return detail::argcmp(
        first, last, filter, [](auto u, auto v) { return u < v; });
}

template <class Array, class Predicate>
constexpr scicpp_pure auto argmin(const Array &a, Predicate filter) {
    return argmin(a.cbegin(), a.cend(), filter);
}

template <class Array>
constexpr scicpp_pure auto argmin(const Array &a) {
    return argmin(a.cbegin(), a.cend(), filters::all);
}

template <class Array>
constexpr scicpp_pure auto nanargmin(const Array &a) {
    return argmin(a.cbegin(), a.cend(), filters::not_nan);
}

//---------------------------------------------------------------------------------
// argwhere, nonzero
//---------------------------------------------------------------------------------

template <class InputIt, class Predicate>
constexpr auto argwhere(InputIt first, InputIt last, Predicate filter) {
    using IteratorType = std::iterator_traits<InputIt>::value_type;
    using IdxTp = std::iterator_traits<InputIt>::difference_type;

    static_assert(meta::is_predicate<Predicate, IteratorType>);

    std::vector<IdxTp> res{};
    res.reserve(std::size_t(std::distance(first, last)));

    for (auto it = first; it != last; ++it) {
        if (filter(*it)) {
            res.push_back(std::distance(first, it));
        }
    }

    return res;
}

template <class Array, class Predicate>
auto argwhere(const Array &a, Predicate filter) {
    return argwhere(a.cbegin(), a.cend(), filter);
}

template <class Array>
auto nonzero(const Array &a) {
    return argwhere(a.cbegin(), a.cend(), filters::not_zero);
}

} // namespace scicpp

#endif // SCICPP_CORE_NUMERIC
