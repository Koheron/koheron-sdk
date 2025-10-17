#ifndef __SERVER_UTILITIES_REFLECTIONS_HPP__
#define __SERVER_UTILITIES_REFLECTIONS_HPP__

#include <string_view>

// ----------------------------------------------------------
// Type name

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
template<class T>
consteval std::string_view short_type_name() {
    auto s = type_name<T>();
    auto pos = s.rfind("::");
    return (pos == std::string_view::npos) ? s : s.substr(pos + 2);
}

// ----------------------------------------------------------
// Tests
//
// __PRETTY_FUNCTION__ is not a guaranteed stable interface.
// For the infos we extract it should be stable enough,
// but put tests to ensure nothing breaks between GCC versions

namespace test {class Object;}

static_assert(type_name<test::Object>() == "test::Object");
static_assert(short_type_name<test::Object>() == "Object");

#endif // __SERVER_UTILITIES_REFLECTIONS_HPP__
