// SPDX-License-Identifier: MIT
// Copyright (c) 2019-2021 Thomas Vanderbruggen <th.vanderbruggen@gmail.com>

#ifndef SCICPP_CORE_META
#define SCICPP_CORE_META

#include <Eigen/Dense>
#include <array>
#include <complex>
#include <cstdint>
#include <cstdlib>
#include <ratio>
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
// is_iterable
//---------------------------------------------------------------------------------

// https://stackoverflow.com/questions/13830158/check-if-a-variable-is-iterable

namespace detail {

// To allow ADL with custom begin/end
using std::begin;
using std::end;

template <typename T>
auto is_iterable_impl(int)
    -> decltype(begin(std::declval<T &>()) !=
                    end(std::declval<T &>()), // begin/end and operator !=
                void(),                       // Handle evil operator ,
                ++std::declval<
                    decltype(begin(std::declval<T &>())) &>(), // operator ++
                void(*begin(std::declval<T &>())),             // operator*
                std::true_type{});

template <typename T>
std::false_type is_iterable_impl(...);

template <typename T>
using is_iterable = decltype(detail::is_iterable_impl<T>(0));

} // namespace detail

template <typename T>
constexpr bool is_iterable_v = detail::is_iterable<T>::value;

template <typename... T>
using disable_if_iterable = std::enable_if_t<(!is_iterable_v<T> || ...), int>;

template <typename... T>
using enable_if_iterable = std::enable_if_t<(is_iterable_v<T> && ...), int>;

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
    -> decltype(void(std::declval<void (&)(To)>()(std::declval<From>())),
                std::true_type{});

template <class, class>
auto test_implicitly_convertible(...) -> std::false_type;

} // namespace detail

template <class From, class To>
constexpr bool is_implicitly_convertible_v =
    decltype(detail::test_implicitly_convertible<From, To>(0))::value;

} // namespace scicpp::meta

#endif // SCICPP_CORE_META