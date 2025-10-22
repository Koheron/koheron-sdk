/// (c) Koheron

#ifndef __SERVER_RUNTIME_META_UTILS_HPP__
#define __SERVER_RUNTIME_META_UTILS_HPP__

#include <array>
#include <utility>
#include <complex>
#include <cstdint>
#include <tuple>
#include <span>
#include <optional>
#include <type_traits>
#include <vector>

// ----------------------------------------------------------------------------
// Range integer sequence
// ----------------------------------------------------------------------------

// http://stackoverflow.com/questions/35625079/offset-for-variadic-template-integer-sequence
template <std::size_t O, std::size_t... Is>
std::index_sequence<(O + Is)...> add_offset(std::index_sequence<Is...>)
{ return {}; }

template <std::size_t O, std::size_t N>
auto make_index_sequence_with_offset() {
    return add_offset<O>(std::make_index_sequence<N>{});
}

template <std::size_t First, std::size_t Last>
auto make_index_sequence_in_range() {
    return make_index_sequence_with_offset<First, Last - First>();
}

// ----------------------------------------------------------------------------
// Tuple utilities
// ----------------------------------------------------------------------------

template<class T> struct is_std_tuple : std::false_type {};
template<class... U> struct is_std_tuple<std::tuple<U...>> : std::true_type {};
template<class T> inline constexpr bool is_std_tuple_v = is_std_tuple<std::remove_cv_t<T>>::value;

// Index of type in tuple

// http://stackoverflow.com/questions/18063451/get-index-of-a-tuple-elements-type
template <class T, class Tuple>
struct tuple_index;

template <class T, class... Types>
struct tuple_index<T, std::tuple<T, Types...>> {
    static constexpr std::size_t value = 0;
};

template <class T, class U, class... Types>
struct tuple_index<T, std::tuple<U, Types...>> {
    static constexpr std::size_t value = 1 + tuple_index<T, std::tuple<Types...>>::value;
};

template <class T, class Tuple>
constexpr std::size_t tuple_index_v = tuple_index<T, Tuple>::value;

static_assert(tuple_index_v<uint32_t, std::tuple<uint32_t, float>> == 0);
static_assert(tuple_index_v<float, std::tuple<uint32_t, float>> == 1);

// Tuple contains

template<class T, class Tuple>
struct _tuple_contains;

template<class T, class... Ts>
struct _tuple_contains<T, std::tuple<Ts...>>
    : std::bool_constant<(std::is_same_v<T, Ts> || ...)> {};
template<class T, class Tuple>
inline constexpr bool tuple_contains_v = _tuple_contains<T, Tuple>::value;

// ----------------------------------------------------------------------------
// PMF (pointer-to-member function) traits
// ----------------------------------------------------------------------------

template<class> struct pmf_traits;

template<class C, class R, class... A>
struct pmf_traits<R (C::*)(A...)> {
    using clazz = C;
    using ret   = R;
    using args  = std::tuple<A...>;
};

template<class C, class R, class... A>
struct pmf_traits<R (C::*)(A...) const> {
    using clazz = const C;
    using ret   = R;
    using args  = std::tuple<A...>;
};

// ----------------------------------------------------------------------------
// std::complex
// ----------------------------------------------------------------------------

template<class T> struct is_std_complex : std::false_type {};
template<class U> struct is_std_complex<std::complex<U>> : std::true_type {};
template<class T> inline constexpr bool is_std_complex_v = is_std_complex<std::remove_cv_t<T>>::value;

// ----------------------------------------------------------------------------
// Units utilities
// ----------------------------------------------------------------------------

#include <scicpp/core.hpp>

// --- primary template: leave T unchanged ---
template<class T, class = void>
struct strip_units { using type = T; };

// detect scicpp quantity via availability of representation_t<T>
template<class T>
struct strip_units<T, std::void_t<typename scicpp::units::representation_t<T>>> {
  using type = typename scicpp::units::representation_t<T>;
};

// std::array<T, N>
template<class T, std::size_t N>
struct strip_units<std::array<T,N>> {
  using type = std::array<typename strip_units<T>::type, N>;
};

// std::complex<T>
template<class T>
struct strip_units<std::complex<T>> {
  using type = std::complex<typename strip_units<T>::type>;
};

// std::vector<T, Alloc>
template<class T, class Alloc>
struct strip_units<std::vector<T,Alloc>> {
  using U       = typename strip_units<T>::type;
  using NewAlloc= typename std::allocator_traits<Alloc>::template rebind_alloc<U>;
  using type    = std::vector<U, NewAlloc>;
};

// std::optional<T>
template<class T>
struct strip_units<std::optional<T>> {
  using type = std::optional<typename strip_units<T>::type>;
};

// std::pair<A,B>
template<class A, class B>
struct strip_units<std::pair<A,B>> {
  using type = std::pair<typename strip_units<A>::type, typename strip_units<B>::type>;
};

// std::tuple<Ts...>
template<class... Ts>
struct strip_units<std::tuple<Ts...>> {
  using type = std::tuple<typename strip_units<Ts>::type...>;
};

// std::span<T, Extent>  (for API typing only; OK to map element)
template<class T, std::size_t Extent>
struct strip_units<std::span<T,Extent>> {
  using type = std::span<typename strip_units<T>::type, Extent>;
};

// helper alias
template<class T>
using strip_units_t = typename strip_units<T>::type;

#endif // __SERVER_RUNTIME_META_UTILS_HPP__