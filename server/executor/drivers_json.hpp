#ifndef __SERVER_CORE_DRIVERS_JSON_HPP__
#define __SERVER_CORE_DRIVERS_JSON_HPP__

#include "server/utilities/reflections.hpp"
#include "server/utilities/meta_utils.hpp"
#include "server/executor/driver_adapter.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <sstream>
#include <utility>
#include <tuple>
#include <type_traits>
#include <typeinfo>
#include <array>
#include <cxxabi.h>
#include <cstdlib>

namespace koheron {

// width-aware name for integral types (except enums/bool)
template<typename U>
static constexpr std::string_view canonical_int_name() {
    static_assert(std::is_integral_v<U>);
    if constexpr (std::is_same_v<U, bool>) {
        return "bool";
    } else if constexpr (std::is_signed_v<U>) {
        if constexpr (sizeof(U) == 1) return "int8_t";
        else if constexpr (sizeof(U) == 2) return "int16_t";
        else if constexpr (sizeof(U) == 4) return "int32_t";
        else if constexpr (sizeof(U) == 8) return "int64_t";
    } else { // unsigned
        if constexpr (sizeof(U) == 1) return "uint8_t";
        else if constexpr (sizeof(U) == 2) return "uint16_t";
        else if constexpr (sizeof(U) == 4) return "uint32_t";
        else if constexpr (sizeof(U) == 8) return "uint64_t";
    }
    // Fallback (very rare: exotic integer widths)
    return "integer";
}

template<typename T>
inline auto get_type_str() {
    using U0 = strip_units_t<std::remove_cv_t<std::remove_reference_t<T>>>;
    using U  = std::remove_cv_t<std::remove_reference_t<U0>>;

    // Prefer explicit, readable names for the common stuff
    if constexpr (std::is_enum_v<U>) {
        // keep the enum's real type name
    } else if constexpr (std::is_integral_v<U>) {
        // map int/unsigned/short/long/... to fixed-width names
        return std::string(canonical_int_name<U>());
    } else if constexpr (std::is_same_v<U, std::string>) {
        return std::string("std::string");
    } else if constexpr (std::is_same_v<U, std::string_view>) {
        return std::string("std::string_view");
    } else if constexpr (std::is_same_v<U, const char*> || std::is_same_v<U, char*>) {
        return std::string("const char *");
    } else if constexpr (std::is_floating_point_v<U>) {
        // keep float/double/long double as-is
        if constexpr (std::is_same_v<U, float>)       return std::string("float");
        if constexpr (std::is_same_v<U, double>)      return std::string("double");
        if constexpr (std::is_same_v<U, long double>) return std::string("long double");
    }
    // Otherwise: fall back to demangled name
    std::string res;
    int status = 0;
    char* name = abi::__cxa_demangle(typeid(U).name(), nullptr, nullptr, &status);
    if (name) { res.assign(name); std::free(name); }
    else      { res = typeid(U).name(); }
    return res;
}

// ------- Small JSON helpers -------
inline void json_escape_into(std::string& out, std::string_view s) {
    for (char c : s) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '\"': out += "\\\""; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
}

inline std::string json_string(std::string_view s) {
    std::string out; out.reserve(s.size() + 4);
    out += '"'; json_escape_into(out, s); out += '"';
    return out;
}

// ------- Adapter meta to expose DriverID, class, ops... -------
template<class> struct adapter_meta; // primary

// Specialization that “opens” DriverAdapter<DriverID, C, Ops...>
template<int DriverID, class C, class... Ops>
struct adapter_meta<DriverAdapter<DriverID, C, Ops...>> {
    static constexpr int id = DriverID;
    using clazz = C;
    using ops_tuple = std::tuple<Ops...>;
    static constexpr std::size_t n_ops = sizeof...(Ops);
};

// ------- Op / PMF meta -------
template<class Op> struct op_meta; // primary

// Pull PMF out of Op<PMF> (no arg names provided)
template<auto PMF>
struct op_meta<Op<PMF>> {
    static constexpr auto pmf = PMF;
    using traits = pmf_traits<decltype(PMF)>;
    using ret    = typename traits::ret;
    using args   = typename traits::args;

    template<std::size_t>
    static constexpr std::string_view arg_name() noexcept { return {}; }
};

// Helper: turn NTTP name object into std::string_view (constexpr-friendly if available)
template <class FS>
inline std::string_view to_sv(const FS& fs) noexcept {
    if constexpr (requires { fs.view(); }) {
        return fs.view();
    } else if constexpr (requires { fs.c_str(); FS::size; }) {
        return std::string_view(fs.c_str(), FS::size);
    } else if constexpr (requires { fs.data(); FS::size; }) {
        return std::string_view(fs.data(), FS::size);
    } else {
        // Last resort: try implicit conversion (runtime OK)
        return std::string_view{fs};
    }
}

// Specialization for Op with NTTP arg names (note: auto... Names)
template<auto PMF, auto... Names>
struct op_meta<Op<PMF, Names...>> {
    static constexpr auto pmf = PMF;
    using traits = pmf_traits<decltype(PMF)>;
    using ret    = typename traits::ret;
    using args   = typename traits::args;

    // Runtime (cached) array avoids forcing constexpr on to_sv(Names)
    static inline const std::array<std::string_view, sizeof...(Names)>&
    arg_names_array() noexcept {
        static const std::array<std::string_view, sizeof...(Names)> names{ to_sv(Names)... };
        return names;
    }

    template<std::size_t I>
    static inline std::string_view arg_name() noexcept {
        constexpr std::size_t N = sizeof...(Names);
        if constexpr (I < N) {
            return arg_names_array()[I];
        } else {
            return {};
        }
    }
};

// Produce the op name "foo" from &C::foo
template<auto PMF>
constexpr std::string_view op_name() noexcept {
    return pmf_unqualified( pmf_full_name<PMF>() );
}

// Produce the class name "C" from &C::foo
template<auto PMF>
constexpr std::string_view op_class() noexcept {
    return pmf_class( pmf_full_name<PMF>() );
}

// ------- Per-op JSON -------
template<class Op, unsigned OpId>
std::string op_to_json() {
    using OM    = op_meta<Op>;
    using Ret   = typename OM::ret;
    using ArgsT = typename OM::args;

    std::string s;
    s += "{\"name\":";           s += json_string(std::string(op_name<OM::pmf>()));
    s += ",\"id\":";             s += std::to_string(OpId);
    s += ",\"ret_type\":";       s += json_string(std::string(get_type_str<Ret>()));
    s += ",\"args\":[";
    [&]<std::size_t... I>(std::index_sequence<I...>) {
        (([&]{
            using ArgI = std::tuple_element_t<I, ArgsT>;
            if constexpr (I > 0) s += ",";
            auto nm = OM::template arg_name<I>(); // runtime OK
            if (nm.empty()) {
                s += "{\"name\":\"arg"; s += std::to_string(I); s += "\",";
            } else {
                s += "{\"name\":"; s += json_string(std::string(nm)); s += ",";
            }
            s += "\"type\":"; s += json_string(std::string(get_type_str<ArgI>()));
            s += "}";
        }()), ...);
    }(std::make_index_sequence<std::tuple_size_v<ArgsT>>{});
    s += "]}";
    return s;
}

// ------- Whole adapter JSON -------
template<class Adapter>
std::string to_json() {
    using AM = adapter_meta<Adapter>;
    using C  = typename AM::clazz;

    std::string s;
    s += "{";
    // "class"
    s += "\"class\":";
    s += json_string( std::string(short_type_name<C>()) );
    s += ",";

    // "id"
    s += "\"id\":";
    s += std::to_string(AM::id);
    s += ",";

    // "functions"
    s += "\"functions\":[";
    if constexpr (AM::n_ops > 0) {
        [&]<std::size_t... I>(std::index_sequence<I...>) {
            (([&]{
                if (I) s += ",";
                using OpI = std::tuple_element_t<I, typename AM::ops_tuple>;
                s += op_to_json<OpI, I>();
            }()), ...);
        }(std::make_index_sequence<AM::n_ops>{});
    }
    s += "]";
    s += "}";
    return s;
}

// ------- Full JSON for all adapters -------
template<class... Adapters>
std::string build_drivers_json() {
    std::string out;
    out += "[";

    // KServer block first
    out += R"({"class":"KServer","id":1,"functions":[)";
    out += R"({"name":"get_version","id":0,"args":[],"ret_type":"const char *"},)";
    out += R"({"name":"get_cmds","id":1,"args":[],"ret_type":"std::string"}]})";

    // Append adapters
    ((out += ",", out += to_json<Adapters>()), ...);

    out += "]";
    return out;
}

} // namespace koheron

#endif // __SERVER_CORE_DRIVERS_JSON_HPP__
