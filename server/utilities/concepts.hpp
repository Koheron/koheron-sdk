#ifndef __SERVER_UTILITIES_CONCEPTS_HPP__
#define __SERVER_UTILITIES_CONCEPTS_HPP__

#include <concepts>
#include <ranges>
#include <iterator>
#include <type_traits>
#include <utility>

namespace koheron {

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

} // namespace koheron

#endif // __SERVER_UTILITIES_CONCEPTS_HPP__