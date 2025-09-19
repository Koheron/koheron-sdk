#ifndef __DRIVER_ID_HPP__
#define __DRIVER_ID_HPP__

#include <array>
#include <memory>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#include "meta_utils.hpp"

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

class NoDriver;
class Server;

// Generates the necessary objects from a list of drivers
template<class... Drivers>
struct drivers_table {
    static constexpr std::size_t offset = 2; // 0: NoDriver, 1: Server

    using types = std::tuple<Drivers...>;
    using tuple_t = std::tuple<std::unique_ptr<Drivers>...>;
    static constexpr std::size_t size = offset + sizeof...(Drivers);

    static consteval auto make_names() {
        std::array<std::string_view, size> a{};
        a[0] = "NoDriver";
        a[1] = "Server";
        std::size_t i = offset;
        ((a[i++] = short_name(type_name<Drivers>())), ...);
        return a;
    }

    inline static constexpr auto names = make_names();
    static_assert(std::tuple_size_v<decltype(names)> == size);

    template<class D>
    inline static constexpr driver_id id_of = [] {
        if constexpr (std::is_same_v<D, NoDriver>) {
            return driver_id{0};
        } else if constexpr (std::is_same_v<D, Server>) {
            return driver_id{1};
        } else {
            return driver_id{offset + tuple_index_v<D, types>};
        }
    }();

    template<driver_id id>
    using type_of =
        std::conditional_t<id == 0, NoDriver,
        std::conditional_t<id == 1, Server,
        std::tuple_element_t<id - offset, types>>>;
};

// Adapter: build drivers_table from a std::tuple<Drivers...>
template<class Tuple>
struct drivers_table_from_tuple;

template<class... Drivers>
struct drivers_table_from_tuple<std::tuple<Drivers...>> : drivers_table<Drivers...> {};

template<class Tuple>
using drivers_table_t = drivers_table_from_tuple<Tuple>;

// ----------------------------------------------------------
// Forward declarations

template<class Driver> Driver& get_driver();

} // namespace koheron

#endif // __DRIVER_ID_HPP__