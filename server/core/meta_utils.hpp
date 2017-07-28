/// (c) Koheron

#ifndef __META_UTILS_HPP__
#define __META_UTILS_HPP__

#include <utility>
#include <cstdint>

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
struct Index;

template <class T, class... Types>
struct Index<T, std::tuple<T, Types...>> {
    static constexpr std::size_t value = 0;
};

template <class T, class U, class... Types>
struct Index<T, std::tuple<U, Types...>> {
    static constexpr std::size_t value = 1 + Index<T, std::tuple<Types...>>::value;
};

template <class T, class Tuple>
constexpr std::size_t Index_v = Index<T, Tuple>::value;

static_assert(Index_v<uint32_t, std::tuple<uint32_t, float>> == 0, "");
static_assert(Index_v<float, std::tuple<uint32_t, float>> == 1, "");

#endif // __META_UTILS_HPP__