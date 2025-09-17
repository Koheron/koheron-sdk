#ifndef __DRIVERS_UTILS_HPP__
#define __DRIVERS_UTILS_HPP__

#include <array>
#include <memory>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

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

}

#endif // __DRIVERS_UTILS_HPP__