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
#include <cstddef>
#include <functional>
#include <iterator>
#include <numeric>
#include <type_traits>
#include <vector>

namespace scicpp {

//---------------------------------------------------------------------------------
// sum
//---------------------------------------------------------------------------------

template <class InputIt, class Predicate>
constexpr auto sum(InputIt first, InputIt last, Predicate filter) {
    return filter_reduce_associative(first, last, std::plus<>(), filter);
}

template <class InputIt>
constexpr auto sum(InputIt first, InputIt last) {
    return std::get<0>(sum(first, last, filters::all));
}

template <class Array, class Predicate>
constexpr auto sum(const Array &f, Predicate filter) {
    return sum(f.cbegin(), f.cend(), filter);
}

template <class Array>
constexpr auto sum(const Array &f) {
    return std::get<0>(sum(f, filters::all));
}

template <class Array>
auto nansum(const Array &f) {
    return sum(f, filters::not_nan);
}

//---------------------------------------------------------------------------------
// prod
//---------------------------------------------------------------------------------

template <class InputIt, class Predicate>
constexpr auto prod(InputIt first, InputIt last, Predicate filter) {
    using T = typename std::iterator_traits<InputIt>::value_type;
    return filter_reduce_associative(
        first, last, std::multiplies<>(), filter, T{1});
}

template <class InputIt>
constexpr auto prod(InputIt first, InputIt last) {
    return std::get<0>(prod(first, last, filters::all));
}

template <class Array, class Predicate>
constexpr auto prod(const Array &f, Predicate filter) {
    return prod(f.cbegin(), f.cend(), filter);
}

template <class Array>
constexpr auto prod(const Array &f) {
    return prod(f.cbegin(), f.cend());
}

template <class Array>
auto nanprod(const Array &f) {
    return prod(f, filters::not_nan);
}

//---------------------------------------------------------------------------------
// cumsum
//---------------------------------------------------------------------------------

template <class Array>
auto cumsum(Array &&a) {
    std::partial_sum(a.cbegin(), a.cend(), a.begin());
    return std::move(a);
}

template <class Array>
auto cumsum(const Array &a) {
    return cumsum(Array(a));
}

template <typename T>
auto nancumsum(const std::vector<T> &v) {
    return cumacc(v, std::plus<>(), filters::not_nan);
}

//---------------------------------------------------------------------------------
// cumprod
//---------------------------------------------------------------------------------

template <class Array>
auto cumprod(Array &&a) {
    std::partial_sum(a.cbegin(), a.cend(), a.begin(), std::multiplies<>());
    return std::move(a);
}

template <class Array>
auto cumprod(const Array &a) {
    return cumprod(Array(a));
}

template <typename T>
auto nancumprod(const std::vector<T> &v) {
    return cumacc(v, std::multiplies<>(), filters::not_nan);
}

//---------------------------------------------------------------------------------
// trapz
//---------------------------------------------------------------------------------

template <class InputIt,
          typename T1 = typename std::iterator_traits<InputIt>::value_type,
          typename T2>
constexpr auto trapz(InputIt first, InputIt last, T2 dx) {
    using ret_t = decltype(std::declval<T1>() * std::declval<T2>());
    using raw_t = units::representation_t<ret_t>;
    using dx_t = std::conditional_t<units::is_quantity_v<T2>, T2, raw_t>;

    if (std::distance(first, last) == 0) {
        return ret_t(raw_t{0});
    }

    return raw_t{0.5} * dx_t(dx) *
           (*first + raw_t{2} * sum(first + 1, last - 1) + *(last - 1));
}

template <class Array, typename T>
constexpr auto trapz(const Array &f, T dx) {
    return trapz(f.cbegin(), f.cend(), dx);
}

//---------------------------------------------------------------------------------
// diff
//---------------------------------------------------------------------------------

namespace detail {

template <typename T, std::size_t N>
auto diff_once(const std::array<T, N> &a) {
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
auto diff(const std::array<T, N> &a) {
    static_assert(n >= 0);

    if constexpr (n == 0) {
        return std::array{a};
    } else {
        return diff<n - 1>(detail::diff_once(a));
    }
}

template <typename T, std::size_t N>
auto diff(const std::array<T, N> &a) {
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

template <class InputItLhs, class InputItRhs, class ProductOp>
constexpr scicpp_pure auto inner(InputItLhs first1,
                                 InputItLhs last1,
                                 InputItRhs first2,
                                 InputItRhs last2,
                                 ProductOp op) {
    using T = std::invoke_result_t<
        ProductOp,
        typename std::iterator_traits<InputItLhs>::value_type,
        typename std::iterator_traits<InputItRhs>::value_type>;

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

template <class InputItLhs, class InputItRhs>
constexpr auto inner(InputItLhs first1,
                     InputItLhs last1,
                     InputItRhs first2,
                     InputItRhs last2) {
    return inner(first1, last1, first2, last2, std::multiplies<>());
}

template <class Array1, class Array2>
constexpr auto inner(const Array1 &a1, const Array2 &a2) {
    return inner(a1.cbegin(), a1.cend(), a2.cbegin(), a2.cend());
}

// inner and dot are the same for 1D arrays
template <class Array1, class Array2>
constexpr auto dot(const Array1 &a1, const Array2 &a2) {
    return inner(a1, a2);
}

template <class InputItLhs, class InputItRhs>
constexpr auto
vdot(InputItLhs first1, InputItLhs last1, InputItRhs first2, InputItRhs last2) {
    return inner(first1, last1, first2, last2, [](auto x1, auto x2) {
        if constexpr (meta::is_complex_v<decltype(x1)>) {
            return std::conj(x1) * x2;
        } else {
            return x1 * x2;
        }
    });
}

template <class ArrayLhs, class ArrayRhs>
constexpr auto vdot(const ArrayLhs &a1, const ArrayRhs &a2) {
    return vdot(a1.cbegin(), a1.cend(), a2.cbegin(), a2.cend());
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
    meta::is_iterable_v<T> && !meta::is_eigen_container_v<T>;

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
auto operator-(Array &&a) {
    return map(std::negate<>(), std::forward<Array>(a));
}

// logical not

template <class Array, detail::enable_if_operator_iterable<Array> = 0>
auto operator!(Array &&a) {
    return map(std::logical_not<>(), std::forward<Array>(a));
}

// scalar compare
template <class Array,
          typename T = typename Array::value_type,
          detail::enable_if_operator_iterable<Array> = 0,
          detail::enable_if_scalar<T> = 0>
auto operator==(Array &&a, T scalar) {
    return map([=](auto v) { return v == scalar; }, std::forward<Array>(a));
}

template <class Array,
          typename T = typename Array::value_type,
          detail::enable_if_operator_iterable<Array> = 0,
          detail::enable_if_scalar<T> = 0>
auto operator==(T scalar, Array &&a) {
    return a == scalar;
}

template <class Array,
          typename T = typename Array::value_type,
          detail::enable_if_operator_iterable<Array> = 0,
          detail::enable_if_scalar<T> = 0>
auto operator!=(Array &&a, T scalar) {
    return !(a == scalar);
}

template <class Array,
          typename T = typename Array::value_type,
          detail::enable_if_operator_iterable<Array> = 0,
          detail::enable_if_scalar<T> = 0>
auto operator!=(T scalar, Array &&a) {
    return !(a == scalar);
}

template <class Array,
          typename T = typename Array::value_type,
          detail::enable_if_operator_iterable<Array> = 0,
          detail::enable_if_scalar<T> = 0>
auto operator<(Array &&a, T scalar) {
    return map([=](auto v) { return v < scalar; }, std::forward<Array>(a));
}

template <class Array,
          typename T = typename Array::value_type,
          detail::enable_if_operator_iterable<Array> = 0,
          detail::enable_if_scalar<T> = 0>
auto operator>=(Array &&a, T scalar) {
    return !(a < scalar);
}

template <class Array,
          typename T = typename Array::value_type,
          detail::enable_if_operator_iterable<Array> = 0,
          detail::enable_if_scalar<T> = 0>
auto operator<(T scalar, Array &&a) {
    return map([=](auto v) { return scalar < v; }, std::forward<Array>(a));
}

template <class Array,
          typename T = typename Array::value_type,
          detail::enable_if_operator_iterable<Array> = 0,
          detail::enable_if_scalar<T> = 0>
auto operator>=(T scalar, Array &&a) {
    return !(scalar < a);
}

template <class Array,
          typename T = typename Array::value_type,
          detail::enable_if_operator_iterable<Array> = 0,
          detail::enable_if_scalar<T> = 0>
auto operator<=(Array &&a, T scalar) {
    return !(scalar < a);
}

template <class Array,
          typename T = typename Array::value_type,
          detail::enable_if_operator_iterable<Array> = 0,
          detail::enable_if_scalar<T> = 0>
auto operator<=(T scalar, Array &&a) {
    return !(a < scalar);
}

template <class Array,
          typename T = typename Array::value_type,
          detail::enable_if_operator_iterable<Array> = 0,
          detail::enable_if_scalar<T> = 0>
auto operator>(Array &&a, T scalar) {
    return !(a <= scalar);
}

template <class Array,
          typename T = typename Array::value_type,
          detail::enable_if_operator_iterable<Array> = 0,
          detail::enable_if_scalar<T> = 0>
auto operator>(T scalar, Array &&a) {
    return !(scalar <= a);
}

// scalar multiply
template <class Array,
          typename T = typename Array::value_type,
          detail::enable_if_operator_iterable<Array> = 0,
          detail::enable_if_scalar<T> = 0>
auto operator*(Array &&a, T scalar) {
    return map([=](auto v) { return scalar * v; }, std::forward<Array>(a));
}

template <class Array,
          typename T = typename Array::value_type,
          detail::enable_if_operator_iterable<Array> = 0,
          detail::enable_if_scalar<T> = 0>
auto operator*(T scalar, Array &&a) {
    return map([=](auto v) { return scalar * v; }, std::forward<Array>(a));
}

// scalar add

template <class Array,
          typename T = typename Array::value_type,
          detail::enable_if_operator_iterable<Array> = 0,
          detail::enable_if_scalar<T> = 0>
auto operator+(Array &&a, T scalar) {
    return map([=](auto v) { return scalar + v; }, std::forward<Array>(a));
}

template <class Array,
          typename T = typename Array::value_type,
          detail::enable_if_operator_iterable<Array> = 0,
          detail::enable_if_scalar<T> = 0>
auto operator+(T scalar, Array &&a) {
    return map([=](auto v) { return scalar + v; }, std::forward<Array>(a));
}

// scalar substract

template <class Array,
          typename T = typename Array::value_type,
          detail::enable_if_operator_iterable<Array> = 0,
          detail::enable_if_scalar<T> = 0>
auto operator-(Array &&a, T scalar) {
    return map([=](auto v) { return v - scalar; }, std::forward<Array>(a));
}

template <class Array,
          typename T = typename Array::value_type,
          detail::enable_if_operator_iterable<Array> = 0,
          detail::enable_if_scalar<T> = 0>
auto operator-(T scalar, Array &&a) {
    return map([=](auto v) { return scalar - v; }, std::forward<Array>(a));
}

// scalar divide

template <class Array,
          typename T = typename Array::value_type,
          detail::enable_if_operator_iterable<Array> = 0,
          detail::enable_if_scalar<T> = 0>
auto operator/(Array &&a, T scalar) {
    return map([=](auto v) { return v / scalar; }, std::forward<Array>(a));
}

template <class Array,
          typename T = typename Array::value_type,
          detail::enable_if_operator_iterable<Array> = 0,
          detail::enable_if_scalar<T> = 0>
auto operator/(T scalar, Array &&a) {
    return map([=](auto v) { return scalar / v; }, std::forward<Array>(a));
}

// scalar modulus

namespace detail {

template <typename T>
auto modulus(T x, T y) {
    using raw_t = units::representation_t<T>;

    if constexpr (std::is_floating_point_v<raw_t>) {
        return T(std::fmod(units::value(x), units::value(y)));
    } else {
        return T(units::value(x) % units::value(y));
    }
}

} // namespace detail

template <class Array,
          typename T = typename Array::value_type,
          detail::enable_if_operator_iterable<Array> = 0,
          detail::enable_if_scalar<T> = 0>
auto operator%(Array &&a, T scalar) {
    return map([=](auto v) { return detail::modulus(v, scalar); },
               std::forward<Array>(a));
}

template <class Array,
          typename T = typename Array::value_type,
          detail::enable_if_operator_iterable<Array> = 0,
          detail::enable_if_scalar<T> = 0>
auto operator%(T scalar, Array &&a) {
    return map([=](auto v) { return detail::modulus(scalar, v); },
               std::forward<Array>(a));
}

template <class ArrayLhs,
          class ArrayRhs,
          detail::enable_if_operator_iterable<ArrayLhs> = 0,
          detail::enable_if_operator_iterable<ArrayRhs> = 0>
auto operator*(ArrayLhs &&a, ArrayRhs &&b) {
    return map(std::multiplies<>(),
               std::forward<ArrayLhs>(a),
               std::forward<ArrayRhs>(b));
}

template <class ArrayLhs,
          class ArrayRhs,
          detail::enable_if_operator_iterable<ArrayLhs> = 0,
          detail::enable_if_operator_iterable<ArrayRhs> = 0>
auto operator&&(ArrayLhs &&a, ArrayRhs &&b) {
    return map(std::logical_and<>(),
               std::forward<ArrayLhs>(a),
               std::forward<ArrayRhs>(b));
}

template <class ArrayLhs,
          class ArrayRhs,
          detail::enable_if_operator_iterable<ArrayLhs> = 0,
          detail::enable_if_operator_iterable<ArrayRhs> = 0>
auto operator||(ArrayLhs &&a, ArrayRhs &&b) {
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
auto operator+(ArrayLhs &&a, ArrayRhs &&b) {
    return map(
        std::plus<>(), std::forward<ArrayLhs>(a), std::forward<ArrayRhs>(b));
}

template <class ArrayLhs,
          class ArrayRhs,
          detail::enable_if_operator_iterable<ArrayLhs> = 0,
          detail::enable_if_operator_iterable<ArrayRhs> = 0>
auto operator-(ArrayLhs &&a, ArrayRhs &&b) {
    return map(
        std::minus<>(), std::forward<ArrayLhs>(a), std::forward<ArrayRhs>(b));
}

template <class ArrayLhs,
          class ArrayRhs,
          detail::enable_if_operator_iterable<ArrayLhs> = 0,
          detail::enable_if_operator_iterable<ArrayRhs> = 0>
auto operator/(ArrayLhs &&a, ArrayRhs &&b) {
    return map(
        std::divides<>(), std::forward<ArrayLhs>(a), std::forward<ArrayRhs>(b));
}

template <class ArrayLhs,
          class ArrayRhs,
          detail::enable_if_operator_iterable<ArrayLhs> = 0,
          detail::enable_if_operator_iterable<ArrayRhs> = 0>
auto operator%(ArrayLhs &&a, ArrayRhs &&b) {
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

template <class ArrayLhs,
          class ArrayRhs,
          meta::enable_if_iterable<ArrayLhs> = 0,
          meta::enable_if_iterable<ArrayRhs> = 0>
auto equal(ArrayLhs &&a, ArrayRhs &&b) {
    return map([](auto u, auto v) { return u == v; },
               std::forward<ArrayLhs>(a),
               std::forward<ArrayRhs>(b));
}

template <class ArrayLhs,
          class ArrayRhs,
          meta::enable_if_iterable<ArrayLhs> = 0,
          meta::enable_if_iterable<ArrayRhs> = 0>
auto not_equal(ArrayLhs &&a, ArrayRhs &&b) {
    using namespace operators;
    return !equal(std::forward<ArrayLhs>(a), std::forward<ArrayRhs>(b));
}

template <class ArrayLhs,
          class ArrayRhs,
          meta::enable_if_iterable<ArrayLhs> = 0,
          meta::enable_if_iterable<ArrayRhs> = 0>
auto less(ArrayLhs &&a, ArrayRhs &&b) {
    return map([](auto u, auto v) { return u < v; },
               std::forward<ArrayLhs>(a),
               std::forward<ArrayRhs>(b));
}

template <class ArrayLhs,
          class ArrayRhs,
          meta::enable_if_iterable<ArrayLhs> = 0,
          meta::enable_if_iterable<ArrayRhs> = 0>
auto less_equal(ArrayLhs &&a, ArrayRhs &&b) {
    using namespace operators;
    return !less(std::forward<ArrayLhs>(b), std::forward<ArrayRhs>(a));
}

template <class ArrayLhs,
          class ArrayRhs,
          meta::enable_if_iterable<ArrayLhs> = 0,
          meta::enable_if_iterable<ArrayRhs> = 0>
auto greater_equal(ArrayLhs &&a, ArrayRhs &&b) {
    using namespace operators;
    return !less(std::forward<ArrayLhs>(a), std::forward<ArrayRhs>(b));
}

template <class ArrayLhs,
          class ArrayRhs,
          meta::enable_if_iterable<ArrayLhs> = 0,
          meta::enable_if_iterable<ArrayRhs> = 0>
auto greater(ArrayLhs &&a, ArrayRhs &&b) {
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
    using IteratorType = typename std::iterator_traits<InputIt>::value_type;
    using IdxTp = typename std::iterator_traits<InputIt>::difference_type;

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
    using IteratorType = typename std::iterator_traits<InputIt>::value_type;
    using IdxTp = typename std::iterator_traits<InputIt>::difference_type;

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
