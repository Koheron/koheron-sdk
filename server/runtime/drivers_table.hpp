#ifndef __DRIVER_ID_HPP__
#define __DRIVER_ID_HPP__

#include <array>
#include <memory>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#include "server/runtime/meta_utils.hpp"

namespace koheron {

using driver_id = std::size_t;

// ----------------------------------------------------------
// drivers_table

template<class T>
consteval std::string_view type_name() {
    std::string_view p = __PRETTY_FUNCTION__;
    // e.g. "consteval std::string_view ... type_name() [with T = ns::Type; std::string_view = std::basic_string_view<char>]"
    constexpr std::string_view key = "T = ";
    auto from = p.find(key);

    if (from == std::string_view::npos) {
        return {};
    }

    from += key.size();
    auto to = p.find_first_of(";]", from);       // stop before alias note or closing bracket
    if (to == std::string_view::npos) {
        to = p.size();
    }

    return p.substr(from, to - from);
}

// Keep only the last component after '::'
consteval std::string_view short_name(std::string_view s) {
    auto pos = s.rfind("::");
    return (pos == std::string_view::npos) ? s : s.substr(pos + 2);
}

template<std::size_t id, bool InSentinels, class SentTuple, class TypesTuple>
struct id_to_type_sel;

// id in sentinels
template<std::size_t id, class... S, class... D>
struct id_to_type_sel<id, true, std::tuple<S...>, std::tuple<D...>> {
    using type = std::tuple_element_t<id, std::tuple<S...>>;
};

// id in drivers
template<std::size_t id, class... S, class... D>
struct id_to_type_sel<id, false, std::tuple<S...>, std::tuple<D...>> {
    using type = std::tuple_element_t<id - sizeof...(S), std::tuple<D...>>;
};

template<std::size_t id, class SentTuple, class TypesTuple>
using id_to_type_t =
    typename id_to_type_sel<
        id,
        (id < std::tuple_size_v<SentTuple>),
        SentTuple,
        TypesTuple
    >::type;

template<class SentinelsTuple, class... Drivers>
struct drivers_table_with_prefix; // primary

template<class... Sentinels, class... Drivers>
struct drivers_table_with_prefix<std::tuple<Sentinels...>, Drivers...> {
    // Offsets / sizes
    static constexpr std::size_t offset = sizeof...(Sentinels);
    static constexpr std::size_t size   = offset + sizeof...(Drivers);

    // Types
    using sentinels_t = std::tuple<Sentinels...>;
    using types       = std::tuple<Drivers...>;                     // only "real" drivers
    using tuple_t     = std::tuple<std::unique_ptr<Drivers>...>;    // storage for drivers

    // Names: [Sentinels..., Drivers...]
    static consteval auto make_names() {
        std::array<std::string_view, size> a{};
        std::size_t i = 0;
        ((a[i++] = short_name(type_name<Sentinels>())), ...);
        ((a[i++] = short_name(type_name<Drivers>())),  ...);
        return a;
    }
    inline static constexpr auto names = make_names();
    static_assert(std::tuple_size_v<decltype(names)> == size);

    // Map TYPE -> id
    template<class D>
    inline static constexpr driver_id id_of = [] {
        if constexpr (tuple_contains_v<D, sentinels_t>) {
            return driver_id{tuple_index_v<D, sentinels_t>};
        } else {
            static_assert(tuple_contains_v<D, types>,
                          "drivers_table::id_of<D>: D not found in sentinels or drivers");
            return driver_id{offset + tuple_index_v<D, types>};
        }
    }();

    // Map id -> TYPE
    template<driver_id id>
    using type_of = id_to_type_t<id, sentinels_t, types>;
};

// ------------------------------------------------------------------
// Backward-compatible alias: no sentinels (offset = 0)
template<class... Drivers>
using drivers_table = drivers_table_with_prefix<std::tuple<>, Drivers...>;

// ------------------------------------------------------------------
// Tuple adapters
template<class Tuple1, class Tuple2 = void>
struct drivers_table_from_tuple;

// 1) Only a drivers tuple
template<class... Drivers>
struct drivers_table_from_tuple<std::tuple<Drivers...>, void>
    : drivers_table<Drivers...> {};

template<class Tuple1, class Tuple2 = void>
using drivers_table_t = drivers_table_from_tuple<Tuple1, Tuple2>;

// 2) Sentinels tuple + drivers tuple
template<class... Sentinels, class... Drivers>
struct drivers_table_from_tuple<std::tuple<Sentinels...>, std::tuple<Drivers...>>
    : drivers_table_with_prefix<std::tuple<Sentinels...>, Drivers...> {};

// ----------------------------------------------------------
// Forward declarations

template<class Driver> Driver& get_driver();

} // namespace koheron

#endif // __DRIVER_ID_HPP__