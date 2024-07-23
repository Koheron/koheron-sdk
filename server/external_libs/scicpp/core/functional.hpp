// SPDX-License-Identifier: MIT
// Copyright (c) 2019-2021 Thomas Vanderbruggen <th.vanderbruggen@gmail.com>

#ifndef SCICPP_CORE_FUNCTIONAL
#define SCICPP_CORE_FUNCTIONAL

#include "scicpp/core/macros.hpp"
#include "scicpp/core/meta.hpp"
#include "scicpp/core/units/maths.hpp"
#include "scicpp/core/units/quantity.hpp"
#include "scicpp/core/utils.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <iterator>
#include <numeric>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace scicpp {

//---------------------------------------------------------------------------------
// map
//---------------------------------------------------------------------------------

// Unary operations

template <class Array, class UnaryOp>
[[nodiscard]] auto map(UnaryOp op, Array &&a) {
    using InputType = typename std::remove_reference_t<Array>::value_type;
    using ReturnType = std::invoke_result_t<UnaryOp, InputType>;

    if constexpr (std::is_same_v<InputType, ReturnType>) {
        std::transform(a.cbegin(), a.cend(), a.begin(), op);
        return std::move(a);
    } else {
        auto res = utils::set_array<ReturnType>(a);
        std::transform(a.cbegin(), a.cend(), res.begin(), op);
        return res;
    }
}

template <class Array, class UnaryOp>
[[nodiscard]] auto map(UnaryOp op, const Array &a) {
    using InputType = typename Array::value_type;
    using ReturnType = std::invoke_result_t<UnaryOp, InputType>;

    auto res = utils::set_array<ReturnType>(a);
    std::transform(a.cbegin(), a.cend(), res.begin(), op);
    return res;
}

// Binary operations

// Array can be of different types (ex. std::array and std::vector),
// data types can also be different (ex. complex and doubles).
//
// However, arrays must have the same size.

template <class Array1,
          class Array2,
          class BinaryOp,
          std::enable_if_t<!std::is_lvalue_reference_v<Array1>, int> = 0>
[[nodiscard]] auto map(BinaryOp op, Array1 &&a1, const Array2 &a2) {
    using InputType1 = typename Array1::value_type;
    using InputType2 = typename Array2::value_type;
    using ReturnType = std::invoke_result_t<BinaryOp, InputType1, InputType2>;

    scicpp_require(a1.size() == a2.size());

    if constexpr (std::is_same_v<InputType1, ReturnType>) {
        std::transform(a1.cbegin(), a1.cend(), a2.cbegin(), a1.begin(), op);
        return std::move(a1);
    } else {
        auto res = utils::set_array<ReturnType>(a1);
        std::transform(a1.cbegin(), a1.cend(), a2.cbegin(), res.begin(), op);
        return res;
    }
}

template <class Array1,
          class Array2,
          class BinaryOp,
          std::enable_if_t<!std::is_lvalue_reference_v<Array2>, int> = 0>
[[nodiscard]] auto map(BinaryOp op, const Array1 &a1, Array2 &&a2) {
    using InputType1 = typename Array1::value_type;
    using InputType2 = typename Array2::value_type;
    using ReturnType = std::invoke_result_t<BinaryOp, InputType1, InputType2>;

    scicpp_require(a1.size() == a2.size());

    if constexpr (std::is_same_v<InputType2, ReturnType>) {
        std::transform(a1.cbegin(), a1.cend(), a2.cbegin(), a2.begin(), op);
        return std::move(a2);
    } else {
        auto res = utils::set_array<ReturnType>(a2);
        std::transform(a1.cbegin(), a1.cend(), a2.cbegin(), res.begin(), op);
        return res;
    }
}

template <class Array1,
          class Array2,
          class BinaryOp,
          std::enable_if_t<!std::is_lvalue_reference_v<Array1> &&
                               !std::is_lvalue_reference_v<Array2>,
                           int> = 0>
[[nodiscard]] auto map(BinaryOp op, Array1 &&a1, Array2 &&a2) {
    using InputType1 = typename Array1::value_type;
    using InputType2 = typename Array2::value_type;
    using ReturnType = std::invoke_result_t<BinaryOp, InputType1, InputType2>;

    if constexpr (std::is_same_v<InputType2, ReturnType>) {
        return map(op, a1, std::move(a2));
    } else {
        return map(op, std::move(a1), a2);
    }
}

template <class Array1, class Array2, class BinaryOp>
[[nodiscard]] auto map(BinaryOp op, const Array1 &a1, const Array2 &a2) {
    return map(op, Array1(a1), a2);
}

//---------------------------------------------------------------------------------
// vectorize
//---------------------------------------------------------------------------------

// Ex. compute sqrt(x), where x is a vector:
// => Using vectorized functions
// https://godbolt.org/z/K81qGjPPr
// => Using raw loop:
// https://godbolt.org/z/aMh5zhT4G
//
// Generate almost the same assembly with both GCC and CLANG.

// Ex. compute cos(sin(x)), where x is a vector:
// => Using vectorized functions
// https://godbolt.org/z/4c3M7zfP6
// => Using raw loop:
// https://godbolt.org/z/Eqcc3ce3W
//
// Both codes generate a single call to new, so vectorize doesn't produce
// unecessary temporaries.
// For the vectorized version first the sin loop is called, then the cos one.
// For the raw loop version a single loop call sin and cos.

template <class Func>
auto vectorize(Func &&f) {
    return [&](auto &&...arrays) {
        if constexpr ((meta::is_iterable_v<decltype(arrays)> && ...)) {
            return map(
                [&](auto &&...args) scicpp_const {
                    return std::invoke(std::forward<Func>(f),
                                       std::forward<decltype(args)>(args)...);
                },
                std::forward<decltype(arrays)>(arrays)...);
        } else {
            return std::invoke(std::forward<Func>(f),
                               std::forward<decltype(arrays)>(arrays)...);
        }
    };
}

//---------------------------------------------------------------------------------
// filter
//---------------------------------------------------------------------------------

namespace filters {

constexpr auto all = []([[maybe_unused]] auto v) { return true; };
constexpr auto none = []([[maybe_unused]] auto v) { return false; };
constexpr auto not_nan = [](auto v) { return !units::isnan(v); };

constexpr auto not_zero = [](auto v) {
    return std::fpclassify(units::value(v)) != FP_ZERO;
};

constexpr auto strictly_positive = [](auto v) { return v > 0; };
constexpr auto positive = [](auto v) { return v >= 0; };
constexpr auto strictly_negative = [](auto v) { return v < 0; };
constexpr auto negative = [](auto v) { return v <= 0; };

template <typename T>
struct Trim {
    constexpr Trim(const std::array<T, 2> &limits_,
                   const std::array<bool, 2> &inclusive_)
        : limits(limits_), inclusive(inclusive_) {}

    constexpr bool operator()(T x) const {
        return (inclusive[0] ? x >= limits[0] : x > limits[0]) &&
               (inclusive[1] ? x <= limits[1] : x < limits[1]);
    }

    std::array<T, 2> limits;
    std::array<bool, 2> inclusive;
};

} // namespace filters

// filter does resize the array in a way that depends on runtime arguments
// so we cannot implement it for std::array.

template <typename T, class UnaryPredicate>
[[nodiscard]] auto filter(std::vector<T> &&a, UnaryPredicate p) {
    static_assert(meta::is_predicate<UnaryPredicate, T>);

    const auto i =
        std::remove_if(a.begin(), a.end(), [p](auto v) { return !p(v); });
    a.erase(i, a.end());
    return std::move(a);
}

template <class Array, class UnaryPredicate>
[[nodiscard]] auto filter(const Array &a, UnaryPredicate p) {
    return filter(std::vector(a.cbegin(), a.cend()), p);
}

//---------------------------------------------------------------------------------
// filter_reduce
//---------------------------------------------------------------------------------

template <class InputIt, class UnaryPredicate, class BinaryOp, typename T>
[[nodiscard]] constexpr scicpp_pure auto filter_reduce(
    InputIt first, InputIt last, BinaryOp op, T init, UnaryPredicate filter) {
    using IteratorType = typename std::iterator_traits<InputIt>::value_type;
    using ReturnType = std::invoke_result_t<BinaryOp, T, IteratorType>;

    static_assert(std::is_same_v<ReturnType, T>);
    static_assert(meta::is_predicate<UnaryPredicate, IteratorType>);

    signed_size_t cnt = 0;

    for (; first != last; ++first) {
        if (filter(*first)) {
            init = op(init, *first);
            cnt++;
        }
    }

    return std::tuple{init, cnt};
}

template <class Array, class UnaryPredicate, class BinaryOp, typename T2>
[[nodiscard]] constexpr scicpp_pure auto
filter_reduce(const Array &a, BinaryOp op, T2 init, UnaryPredicate filter) {
    return filter_reduce(a.cbegin(), a.cend(), op, init, filter);
}

//---------------------------------------------------------------------------------
// reduce
//---------------------------------------------------------------------------------

template <class Array, class BinaryOp, typename T = typename Array::value_type>
[[nodiscard]] constexpr scicpp_pure auto
reduce(const Array &a, BinaryOp op, T init) {
    return filter_reduce(a, op, init, filters::all);
}

//---------------------------------------------------------------------------------
// pairwise_accumulate
//
// Implements pairwise recursion
// https://en.wikipedia.org/wiki/Pairwise_summation
//
//  | BLOCK | BLOCK | BLOCK | BLOCK | BLOCK | BLOCK | BLOCK | BLOCK |
//      |       |       |       |       |       |       |       |
//     acc     acc     acc     acc     acc     acc     acc     acc
//      |_______|       |_______|       |_______|       |_______|
//          |               |               |               |
//       combine         combine         combine         combine
//          |_______________|               |_______________|
//                  |_______________________________|
//                                  |
//                               combine
//                                  |
//                                Result
//---------------------------------------------------------------------------------

template <signed_size_t PW_BLOCKSIZE,
          class InputIt,
          class AccumulateOp,
          class CombineOp>
[[nodiscard]] constexpr scicpp_pure auto pairwise_accumulate(InputIt first,
                                                             InputIt last,
                                                             AccumulateOp accop,
                                                             CombineOp combop) {
    const auto size = std::distance(first, last);

    if (size <= PW_BLOCKSIZE) {
        return accop(first, last);
    } else {
        return combop(pairwise_accumulate<PW_BLOCKSIZE>(
                          first, first + size / 2, accop, combop),
                      pairwise_accumulate<PW_BLOCKSIZE>(
                          first + size / 2, last, accop, combop));
    }
}

template <signed_size_t PW_BLOCKSIZE,
          class InputItLhs,
          class InputItRhs,
          class AccumulateOp,
          class CombineOp>
[[nodiscard]] constexpr scicpp_pure auto pairwise_accumulate(InputItLhs first1,
                                                             InputItLhs last1,
                                                             InputItRhs first2,
                                                             InputItRhs last2,
                                                             AccumulateOp accop,
                                                             CombineOp combop) {
    const auto size = std::distance(first1, last1);
    scicpp_require(size == std::distance(first2, last2));

    if (size <= PW_BLOCKSIZE) {
        return accop(first1, last1, first2, last2);
    } else {
        return combop(pairwise_accumulate<PW_BLOCKSIZE>(first1,
                                                        first1 + size / 2,
                                                        first2,
                                                        first2 + size / 2,
                                                        accop,
                                                        combop),
                      pairwise_accumulate<PW_BLOCKSIZE>(first1 + size / 2,
                                                        last1,
                                                        first2 + size / 2,
                                                        last2,
                                                        accop,
                                                        combop));
    }
}

//---------------------------------------------------------------------------------
// filter_reduce_associative
//
// Use pairwise recursion for improved precision in associative operations.
//
// https://github.com/numpy/numpy/pull/3685
// https://github.com/JuliaLang/julia/pull/4039
//---------------------------------------------------------------------------------

template <class InputIt,
          class UnaryPredicate,
          class AssociativeBinaryOp,
          typename T = typename std::iterator_traits<InputIt>::value_type>
[[nodiscard]] constexpr scicpp_pure auto
filter_reduce_associative(InputIt first,
                          InputIt last,
                          AssociativeBinaryOp op,
                          UnaryPredicate filter,
                          T id_elt = utils::set_zero<T>()) {
    if constexpr (std::is_integral_v<T>) {
        // No precision problem for integers, as long as you don't overflow ...
        return filter_reduce(first, last, op, id_elt, filter);
    } else {
        return pairwise_accumulate<64>(
            first,
            last,
            [&](auto f, auto l) {
                return filter_reduce(f, l, op, id_elt, filter);
            },
            [&](const auto res1, const auto res2) {
                const auto [x1, n1] = res1;
                const auto [x2, n2] = res2;
                return std::tuple{op(x1, x2), n1 + n2};
            });
    }
}

template <class Array,
          class UnaryPredicate,
          class AssociativeBinaryOp,
          typename T = typename Array::value_type>
[[nodiscard]] constexpr scicpp_pure auto filter_reduce_associative(
    const Array &a, AssociativeBinaryOp op, UnaryPredicate filter) {
    return filter_reduce_associative(a.cbegin(), a.cend(), op, filter);
}

//---------------------------------------------------------------------------------
// cumacc
//---------------------------------------------------------------------------------

template <class Array, class BinaryOp, class UnaryPredicate>
auto cumacc(Array &&a, BinaryOp op, UnaryPredicate p) {
    using InputType = typename std::remove_reference_t<Array>::value_type;
    using ReturnType = std::invoke_result_t<BinaryOp, InputType, InputType>;
    static_assert(meta::is_predicate<UnaryPredicate, InputType>);
    static_assert(std::is_same_v<InputType, ReturnType>);

    auto a_filt = filter(std::forward<Array>(a), p);
    std::partial_sum(a_filt.cbegin(), a_filt.cend(), a_filt.begin(), op);
    return a_filt;
}

} // namespace scicpp

#endif // SCICPP_CORE_FUNCTIONAL