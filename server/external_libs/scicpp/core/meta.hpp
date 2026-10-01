// SPDX-License-Identifier: MIT
// Copyright (c) 2019-2021 Thomas Vanderbruggen <th.vanderbruggen@gmail.com>

#ifndef SCICPP_CORE_META
#define SCICPP_CORE_META

#include "scicpp/core/macros.hpp"

#include <Eigen/Dense>
#include <array>
#include <complex>
#include <concepts>
#include <cstdint>
#include <cstdlib>
#include <ratio>
#include <span>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace scicpp::meta {

//---------------------------------------------------------------------------------
// is_complex
//---------------------------------------------------------------------------------

namespace detail {

template <class T>
struct is_complex : std::false_type {};
template <class T>
struct is_complex<std::complex<T>> : std::true_type {};

} // namespace detail

template <class T>
constexpr bool is_complex_v = detail::is_complex<T>::value;

template <typename T>
using enable_if_complex = std::enable_if_t<is_complex_v<T>, int>;

template <typename T>
using disable_if_complex = std::enable_if_t<!is_complex_v<T>, int>;

//---------------------------------------------------------------------------------
// concept Iterable
//---------------------------------------------------------------------------------

template <class T>
concept Iterable = std::ranges::input_range<std::remove_cvref_t<T>>;

template <class T>
concept NonIterable = !Iterable<T>;

//---------------------------------------------------------------------------------
// std::vector traits
//---------------------------------------------------------------------------------

namespace detail {

template <class T>
struct is_std_vector : std::false_type {};
template <typename Scalar>
struct is_std_vector<std::vector<Scalar>> : std::true_type {};

} // namespace detail

template <class T>
constexpr bool is_std_vector_v = detail::is_std_vector<T>::value;

//---------------------------------------------------------------------------------
// std::array traits
//---------------------------------------------------------------------------------

namespace detail {

template <class T>
struct is_std_array : std::false_type {};
template <typename Scalar, std::size_t N>
struct is_std_array<std::array<Scalar, N>> : std::true_type {};

} // namespace detail

template <class T>
constexpr bool is_std_array_v = detail::is_std_array<T>::value;

//---------------------------------------------------------------------------------
// std::span traits
//---------------------------------------------------------------------------------

namespace detail {

template <class T>
struct is_std_span : std::false_type {};
template <typename Scalar, std::size_t N>
struct is_std_span<std::span<Scalar, N>> : std::true_type {};

} // namespace detail

template <class T>
constexpr bool is_std_span_v = detail::is_std_span<T>::value;

//---------------------------------------------------------------------------------
// std::tuple traits
//---------------------------------------------------------------------------------

namespace detail {

template <class T>
struct is_std_tuple : std::false_type {};
template <typename... Args>
struct is_std_tuple<std::tuple<Args...>> : std::true_type {};

} // namespace detail

template <class T>
constexpr bool is_std_tuple_v = detail::is_std_tuple<T>::value;

//---------------------------------------------------------------------------------
// std::pair traits
//---------------------------------------------------------------------------------

namespace detail {

template <class T>
struct is_std_pair : std::false_type {};
template <typename T1, typename T2>
struct is_std_pair<std::pair<T1, T2>> : std::true_type {};

} // namespace detail

template <class T>
constexpr bool is_std_pair_v = detail::is_std_pair<T>::value;

//---------------------------------------------------------------------------------
// subtuple
// https://stackoverflow.com/questions/17854219/creating-a-sub-tuple-starting-from-a-stdtuplesome-types
//---------------------------------------------------------------------------------

namespace detail {

template <typename... T, std::size_t... I>
constexpr auto subtuple_(const std::tuple<T...> &t,
                         std::index_sequence<I...> /*unused*/) {
    return std::make_tuple(std::get<I>(t)...);
}

} // namespace detail

template <int Trim, typename... T>
constexpr auto subtuple(const std::tuple<T...> &t) {
    return detail::subtuple_(t,
                             std::make_index_sequence<sizeof...(T) - Trim>());
}

template <int Trim = 1, typename T1, typename T2>
constexpr auto subtuple(const std::pair<T1, T2> &t) {
    return std::make_tuple(t.first);
}

//---------------------------------------------------------------------------------
// is_ratio
//---------------------------------------------------------------------------------

namespace detail {

template <class T>
struct is_ratio : std::false_type {};

template <intmax_t num, intmax_t den>
struct is_ratio<std::ratio<num, den>> : std::true_type {};

} // namespace detail

template <class T>
constexpr bool is_ratio_v = detail::is_ratio<T>::value;

//---------------------------------------------------------------------------------
// Eigen type traits
//---------------------------------------------------------------------------------

namespace detail {

template <class T>
struct is_eigen_matrix : std::false_type {};
template <typename Scalar, int RowsAtCompileTime, int ColsAtCompileTime>
struct is_eigen_matrix<
    Eigen::Matrix<Scalar, RowsAtCompileTime, ColsAtCompileTime>>
    : std::true_type {};

template <class T>
struct is_eigen_array : std::false_type {};
template <typename Scalar, int RowsAtCompileTime, int ColsAtCompileTime>
struct is_eigen_array<
    Eigen::Array<Scalar, RowsAtCompileTime, ColsAtCompileTime>>
    : std::true_type {};

} // namespace detail

template <class T>
constexpr bool is_eigen_matrix_v = detail::is_eigen_matrix<T>::value;

template <class T>
constexpr bool is_eigen_array_v = detail::is_eigen_array<T>::value;

template <class T>
constexpr bool is_eigen_container_v =
    is_eigen_matrix_v<T> || is_eigen_array_v<T>;

//---------------------------------------------------------------------------------
// is_predicate
//---------------------------------------------------------------------------------

template <class Predicate, class... Args>
constexpr bool is_predicate =
    std::is_integral_v<std::invoke_result_t<Predicate, Args...>>;

//---------------------------------------------------------------------------------
// is_string
//---------------------------------------------------------------------------------

template <class T, typename Tdecay = typename std::decay_t<T>>
constexpr bool is_string_v =
    std::is_same_v<const char *, Tdecay> || std::is_same_v<char *, Tdecay> ||
    std::is_same_v<std::string, Tdecay>;

//---------------------------------------------------------------------------------
// is_movable
// https://en.cppreference.com/w/cpp/concepts/movable
//---------------------------------------------------------------------------------

template <class T>
constexpr bool is_movable_v =
    std::is_object_v<T> && std::is_move_constructible_v<T> &&
    std::is_assignable_v<T &, T> && std::is_swappable_v<T>;

//---------------------------------------------------------------------------------
// value_type
// https://stackoverflow.com/questions/62203496/type-trait-to-receive-tvalue-type-if-present-t-otherwise
//---------------------------------------------------------------------------------

namespace detail {

template <class T, class = void>
struct value_type {
    using type = T;
};

template <class T>
struct value_type<T, std::void_t<typename T::value_type>> {
    using type = typename T::value_type;
};

} // namespace detail

template <class T>
using value_type_t = typename detail::value_type<T>::type;

//---------------------------------------------------------------------------------
// is_implicitly_convertible
// https://en.cppreference.com/w/cpp/types/is_convertible
//---------------------------------------------------------------------------------

namespace detail {

template <class From, class To>
auto test_implicitly_convertible(int)
    -> decltype(static_cast<void>(
                    (std::declval<void (&)(To)>()(std::declval<From>()))),
                std::true_type{});

template <class, class>
auto test_implicitly_convertible(...) -> std::false_type;

} // namespace detail

template <class From, class To>
constexpr bool is_implicitly_convertible_v =
    decltype(detail::test_implicitly_convertible<From, To>(0))::value;

//---------------------------------------------------------------------------------
// range_size
//---------------------------------------------------------------------------------

namespace detail {

template <class R>
struct range_size : std::integral_constant<std::size_t, std::dynamic_extent> {};

template <class T, std::size_t N>
struct range_size<std::array<T, N>> : std::integral_constant<std::size_t, N> {};

template <class T, std::size_t N>
struct range_size<std::span<T, N>> : std::integral_constant<std::size_t, N> {};

} // namespace detail

template <class T>
constexpr std::size_t range_size_v =
    detail::range_size<std::remove_cvref_t<T>>::value;

} // namespace scicpp::meta

#endif // SCICPP_CORE_META