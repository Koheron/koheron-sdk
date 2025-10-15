#ifndef __SERVER_CORE_DRIVER_OP_HELPER_HPP__
#define __SERVER_CORE_DRIVER_OP_HELPER_HPP__

#include "server/core/driver.hpp"
#include "server/core/commands.hpp"
#include "server/core/session.hpp"

#include <tuple>
#include <type_traits>
#include <string>
#include <array>
#include <vector>

namespace koheron {

struct Command;

template<class... Args>
struct overload_any_t {
  template<class C, class R>
  constexpr auto operator()(R (C::*p)(Args...)) const -> R (C::*)(Args...) { return p; }
  template<class C, class R>
  constexpr auto operator()(R (C::*p)(Args...) const) const -> R (C::*)(Args...) const { return p; }
};
template<class... Args>
constexpr overload_any_t<Args...> select_overload{};

// ---------- Low-level readers ----------

template <typename T>
inline bool read_one(Command& cmd, T& v) {
    auto t = cmd.session->deserialize<T>(cmd);

    if (std::get<0>(t) < 0) {
        return false;
    }

    v = std::get<1>(t);
    return true;
}

template <typename T, std::size_t N>
inline bool read_one(Command& cmd, std::array<T,N>& a) {
    return cmd.session->recv(a, cmd) >= 0;
}

template <typename T>
inline bool read_one(Command& cmd, std::vector<T>& v) {
    return cmd.session->recv(v, cmd) >= 0;
}

inline bool read_one(Command& cmd, std::string& s) {
    return cmd.session->recv(s, cmd) >= 0;
}

template <typename... Ts>
inline bool read_args(Command& cmd, Ts&... ts) {
    bool ok = true;
    (void)std::initializer_list<int>{ (ok = ok && read_one(cmd, ts), 0)... };
    return ok;
}

// ---------- Tuple utilities ----------
template <typename... Args>
inline bool deserialize_tuple(Command& cmd, std::tuple<Args...>& tup) {
    bool ok = true;
    std::apply([&](auto&... elems) {
        (void)std::initializer_list<int>{ (ok = ok && read_one(cmd, elems), 0)... };
    }, tup);
    return ok;
}

template<int DriverID, int OpID, class C, class Ret, class... Args>
int op_invoke(C& obj, Ret (C::*pmf)(Args...), Command& cmd) {
    // How many args arrive on the wire (excluding the status int)
    constexpr std::size_t N = sizeof...(Args);

    // Only assert buffer size when there are arguments
    if constexpr (N > 0) {
        constexpr std::size_t need = required_buffer_size<std::decay_t<Args>...>();
        static_assert(need <= CMD_PAYLOAD_BUFFER_LEN, "Buffer size too small");
    }

    // Deserialize (or fabricate an OK status for zero-arg ops)
    using deser_tuple_t =
        std::conditional_t<N == 0, std::tuple<int>, std::tuple<int, std::decay_t<Args>...>>;

    deser_tuple_t tup = [&] {
        if constexpr (N == 0) {
            return deser_tuple_t{0}; // success status, no payload to read
        } else {
            return cmd.session->deserialize<std::decay_t<Args>...>(cmd);
        }
    }();

    if (std::get<0>(tup) < 0) {
        return -1;
    }

    // Invoke with perfect forwarding. Preserve reference returns with decltype(auto).
    return [&]<std::size_t... I>(std::index_sequence<I...>) -> int {
        // Tail = the arguments (skip status at index 0)
        auto tail = std::forward_as_tuple(std::get<I + 1>(tup)...);

        if constexpr (std::is_void_v<Ret>) {
            std::apply([&](auto&&... a) {
                (obj.*pmf)(std::forward<decltype(a)>(a)...);
            }, tail);
            return 0;
        } else {
            decltype(auto) r = std::apply([&](auto&&... a) -> decltype(auto) {
                return (obj.*pmf)(std::forward<decltype(a)>(a)...);
            }, tail);

            // Forward value or lvalue-ref exactly as returned by the method
            return cmd.session->send<DriverID, OpID>(std::forward<decltype(r)>(r));
        }
    }(std::make_index_sequence<N>{});
}

template<int DriverID, int OpID, typename C, typename Ret, typename... Args>
int op_invoke(const C& obj, Ret (C::*pmf)(Args...) const, Command& cmd) {
    using Tup = std::tuple<std::decay_t<Args>...>;
    Tup args{};
    if (!deserialize_tuple(cmd, args)) {
        return -1;
    }

    if constexpr (std::is_void_v<Ret>) {
        std::apply([&](auto&... elems){
            (obj.*pmf)(elems...);
        }, args);
        return 0;
    } else {
        Ret r = std::apply([&](auto&... elems){
            return (obj.*pmf)(elems...);
        }, args);
        return cmd.session->send<DriverID, OpID>(r);
    }
}

} // namespace koheron

#endif // __SERVER_CORE_DRIVER_OP_HELPER_HPP__
