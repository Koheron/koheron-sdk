#ifndef __SERVER_UTILITIES_CONCEPTS_HPP__
#define __SERVER_UTILITIES_CONCEPTS_HPP__

#include "server/utilities/meta_utils.hpp"

#include <array>
#include <tuple>
#include <concepts>
#include <ranges>
#include <iterator>
#include <type_traits>
#include <utility>

namespace ut {

template<class C>
concept resizableContiguousRange =
std::ranges::contiguous_range<C> &&
std::ranges::sized_range<C> &&
requires (C c, const C cc, std::size_t n) {
    // must be able to grow/shrink
    { c.resize(n) } -> std::same_as<void>;

    // must expose contiguous storage pointers
    { std::data(c)  } -> std::same_as<std::add_pointer_t<std::ranges::range_value_t<C>>>;
    { std::data(cc) } -> std::same_as<std::add_pointer_t<const std::ranges::range_value_t<C>>>;
};

template<class T>
concept ContiguousContainer = requires(T t) {
    typename T::value_type;
    { t.data() } -> std::convertible_to<const typename T::value_type*>;
    { t.size() } -> std::convertible_to<std::size_t>;
};

template<class T>
concept StdArray = requires {
    typename T::value_type;
    std::tuple_size<T>::value; // std::array satisfies
};

template<class T>
concept CStrLike =
    // char* / const char*
    (std::is_pointer_v<std::remove_reference_t<T>> &&
     std::is_same_v<std::remove_cv_t<std::remove_pointer_t<std::remove_reference_t<T>>>, char>) ||
    // char[N] / const char[N]
    (std::is_array_v<std::remove_reference_t<T>> &&
     std::is_same_v<std::remove_cv_t<std::remove_extent_t<std::remove_reference_t<T>>>, char>);

template<class T>
concept ScalarLike = (std::is_scalar_v<std::remove_reference_t<T>> && !std::is_pointer_v<std::remove_reference_t<T>>) || is_std_complex_v<T>;

} // namespace ut

#endif // __SERVER_UTILITIES_CONCEPTS_HPP__