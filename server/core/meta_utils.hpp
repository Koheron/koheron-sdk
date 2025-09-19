/// (c) Koheron

#ifndef __META_UTILS_HPP__
#define __META_UTILS_HPP__

#include <utility>
#include <cstdint>
#include <tuple>
#include <type_traits>

// Range integer sequence

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

template<class T, class Tuple>
struct _tuple_contains;

template<class T, class... Ts>
struct _tuple_contains<T, std::tuple<Ts...>>
    : std::bool_constant<(std::is_same_v<T, Ts> || ...)> {};
template<class T, class Tuple>
inline constexpr bool tuple_contains_v = _tuple_contains<T, Tuple>::value;

#endif // __META_UTILS_HPP__