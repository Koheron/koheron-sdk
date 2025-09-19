#ifndef __DRIVER_ID_HPP__
#define __DRIVER_ID_HPP__

#include <array>
#include <memory>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#include "meta_utils.hpp"

#include <drivers_table.hpp>

namespace koheron {

using driver_id = std::size_t;

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

// Generates the necessary objects from a list of drivers
template<class... Drivers>
struct drivers_table {
    static constexpr std::size_t offset = 2; // 0: NoDriver, 1: Server

    using types = std::tuple<Drivers...>;
    using tuple_t = std::tuple<std::unique_ptr<Drivers>...>;
    static constexpr std::size_t device_num = offset + sizeof...(Drivers);

    static consteval auto make_names() {
        std::array<std::string_view, device_num> a{};
        a[0] = "NoDriver";
        a[1] = "Server";
        std::size_t i = offset;
        ((a[i++] = short_name(type_name<Drivers>())), ...);
        return a;
    }

    inline static constexpr auto names = make_names();
};

// Adapter: build drivers_table from a std::tuple<Drivers...>
template<class Tuple>
struct drivers_table_from_tuple;

template<class... Drivers>
struct drivers_table_from_tuple<std::tuple<Drivers...>> : drivers_table<Drivers...> {};

template<class Tuple>
using drivers_table_t = drivers_table_from_tuple<Tuple>;

// Instantiate driver_table from driver_list

using driver_table = drivers_table_t<driver_list>;
inline constexpr driver_id device_off = driver_table::offset;
inline constexpr driver_id device_num = driver_table::device_num;
inline constexpr auto drivers_names = driver_table::names;
using drivers_tuple_t = typename driver_table::tuple_t;

static_assert(std::tuple_size_v<decltype(drivers_names)> == device_num);

static_assert(std::tuple_size<drivers_tuple_t>::value == device_num - 2, "");

// Driver id from driver type

template<class Driver>
constexpr driver_id driver_id_of = tuple_index_v<std::unique_ptr<Driver>, drivers_tuple_t> + 2;

template<> constexpr driver_id driver_id_of<NoDriver> = 0;
template<> constexpr driver_id driver_id_of<Server> = 1;

// Driver type from driver id

template<driver_id driver>
using device_t = std::remove_reference_t<decltype(*std::get<driver - 2>(std::declval<drivers_tuple_t>()))>;

// Forward declarations

template<class Driver> Driver& get_driver();

} // namespace koheron

#endif // __DRIVER_ID_HPP__