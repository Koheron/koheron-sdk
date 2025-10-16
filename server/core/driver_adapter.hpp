#ifndef __SERVER_CORE_DRIVER_ADAPTER_HPP__
#define __SERVER_CORE_DRIVER_ADAPTER_HPP__

#include "server/core/driver.hpp"
#include "server/core/commands.hpp"
#include "server/core/session.hpp"

#include <array>
#include <type_traits>
#include <utility>
#include <mutex>
#include <tuple>
#include <vector>
#include <string>
#include <initializer_list>

namespace koheron {

// --------- low-level "read one arg" ----------

template <typename T>
inline bool read_one(Command& cmd, T& v) {
    // fixed-size / POD-ish types
    auto t = cmd.session->deserialize<T>(cmd);
    if (std::get<0>(t) < 0) {
        return false;
    }
    v = std::get<1>(t);
    return true;
}

template <typename T>
inline bool read_one(Command& cmd, std::vector<T>& v) {
    return cmd.session->recv(v, cmd) >= 0;   // reads [u32 len][payload...]
}

inline bool read_one(Command& cmd, std::string& s) {
    return cmd.session->recv(s, cmd) >= 0;   // reads [u32 len][bytes...]
}

template <class Tuple, std::size_t... I>
inline bool read_tuple_into(Command& cmd, Tuple& tup, std::index_sequence<I...>) {
    bool ok = true;
    (void)std::initializer_list<int>{ (ok = ok && read_one(cmd, std::get<I>(tup)), 0)... };
    return ok;
}

// ---- PMF (pointer-to-member function) traits ----
template<class> struct pmf_traits;

template<class C, class R, class... A>
struct pmf_traits<R (C::*)(A...)> {
    using clazz = C;
    using ret   = R;
    using args  = std::tuple<A...>;
};

template<class C, class R, class... A>
struct pmf_traits<R (C::*)(A...) const> {
    using clazz = const C;
    using ret   = R;
    using args  = std::tuple<A...>;
};

template<class Tuple> struct prepend_status;

template<class... A>
struct prepend_status<std::tuple<A...>> {
    using type = std::tuple<int, std::decay_t<A>...>;
};

// ---- detect dynamic-size args (vector/string) ----
template<class T> struct is_dynamic_arg : std::false_type {};
template<class U, class A> struct is_dynamic_arg<std::vector<U,A>> : std::true_type {};
template<class Ch, class Tr, class A>
struct is_dynamic_arg<std::basic_string<Ch,Tr,A>> : std::true_type {};

template<class Tuple, std::size_t... I>
consteval bool all_args_static(std::index_sequence<I...>) {
    return (... && !is_dynamic_arg<std::decay_t<std::tuple_element_t<I, Tuple>>>::value);
}

// ---- buffer size computation for fixed-size args ----
template<class Tuple, std::size_t... I>
constexpr std::size_t required_size_over_tuple(std::index_sequence<I...>) {
    return required_buffer_size<std::decay_t<std::tuple_element_t<I, Tuple>>...>();
}

// ---- “make decayed tuple from index sequence” meta ----
template<class Tuple, class Seq> struct decayed_tuple_from_seq;
template<class Tuple, std::size_t... I>
struct decayed_tuple_from_seq<Tuple, std::index_sequence<I...>> {
    using type = std::tuple<std::decay_t<std::tuple_element_t<I, Tuple>>...>;
};

template<int DriverID, int OpID, class Obj, class PMF>
int op_invoke_impl(Obj&& obj, PMF pmf, Command& cmd) {
    using traits     = pmf_traits<PMF>;
    using Ret        = typename traits::ret;
    using ArgsTuple  = typename traits::args;

    constexpr std::size_t N = std::tuple_size_v<ArgsTuple>;

    // Only assert static buffer size if all args are fixed-size
    if constexpr (N > 0 && all_args_static<ArgsTuple>(std::make_index_sequence<N>{})) {
        constexpr auto need = required_size_over_tuple<ArgsTuple>(std::make_index_sequence<N>{});
        static_assert(need <= CMD_PAYLOAD_BUFFER_LEN, "Buffer size too small");
    }

    if constexpr (N == 0) {
        // No payload to read
        if constexpr (std::is_void_v<Ret>) {
            std::invoke(pmf, std::forward<Obj>(obj));
            return 0;
        } else {
            decltype(auto) r = std::invoke(pmf, std::forward<Obj>(obj));
            return cmd.session->send<DriverID, OpID>(std::forward<decltype(r)>(r));
        }
    } else {
        using IS = std::make_index_sequence<N>;
        using DecayedArgsTuple = typename decayed_tuple_from_seq<ArgsTuple, IS>::type;

        DecayedArgsTuple args{};
        if (!read_tuple_into(cmd, args, IS{})) {
            return -1;
        }

        if constexpr (std::is_void_v<Ret>) {
            std::apply([&](auto&... a) {
                std::invoke(pmf, std::forward<Obj>(obj), a...);
            }, args);
            return 0;
        } else {
            decltype(auto) r = std::apply([&](auto&... a) -> decltype(auto) {
                return std::invoke(pmf, std::forward<Obj>(obj), a...);
            }, args);
            return cmd.session->send<DriverID, OpID>(std::forward<decltype(r)>(r));
        }
    }
}

// Non-const member functions
template<int DriverID, int OpID, class C, class Ret, class... Args>
int op_invoke(C& obj, Ret (C::*pmf)(Args...), Command& cmd) {
    return op_invoke_impl<DriverID, OpID>(obj, pmf, cmd);
}

// const member functions
template<int DriverID, int OpID, class C, class Ret, class... Args>
int op_invoke(const C& obj, Ret (C::*pmf)(Args...) const, Command& cmd) {
    return op_invoke_impl<DriverID, OpID>(obj, pmf, cmd);
}

// op id is the position in the pack
template<auto PMF>
struct Op {
    static constexpr auto pmf = PMF;
};

// Force a specific id
template<auto PMF, int FixedID>
struct OpAt {
    static constexpr auto pmf = PMF;
    static constexpr int  id  = FixedID;
};

template<class C, int DriverID, auto PMF, int OpID>
struct OpThunk {
    static int call(C& obj, Command& cmd) {
        return op_invoke<DriverID, OpID>(obj, PMF, cmd);
    }
};

template<class T, class = void>
struct has_id : std::false_type {};

template<class T>
struct has_id<T, std::void_t<decltype(T::id)>> : std::true_type {};

template<int DriverID, class C, class... Ops>
class DriverAdapter : public DriverAbstract {
  public:
    explicit DriverAdapter(C& impl) : DriverAbstract(DriverID), obj(impl) {}

    int execute(Command& cmd) {
        std::lock_guard<std::mutex> lock(mutex_);
        const int op = static_cast<int>(cmd.operation);
        if (op < 0 || op >= static_cast<int>(table_.size())) return -1;
        auto* fn = table_[op];
        return fn ? fn(obj, cmd) : -1;
    }

  private:
    C& obj;
    std::mutex mutex_;

    using Entry = int(*)(C&, Command&);

    template<std::size_t I, class Op>
    static constexpr int op_id_for() {
        if constexpr (has_id<Op>::value) return Op::id;
        else return static_cast<int>(I);     // position == id
    }

    template<std::size_t... I>
    static constexpr int max_id(std::index_sequence<I...>) {
        int m = -1;
        ((m = (op_id_for<I, Ops>() > m ? op_id_for<I, Ops>() : m)), ...);
        return m;
    }

    template<std::size_t... I>
    static constexpr auto make_table(std::index_sequence<I...>) {
        // size is max_id + 1; default-init to nullptrs (OK in constexpr)
        constexpr int N = max_id(std::index_sequence<I...>{}) + 1;
        std::array<Entry, N> a{}; // all nullptr

        // Assign each slot via a fold; no loops -> constexpr friendly.
        (( a[ op_id_for<I, Ops>() ] =
              &OpThunk<C, DriverID, Ops::pmf, op_id_for<I, Ops>()>::call ), ...);

        return a;
    }

    static constexpr auto table_ =
        make_table(std::make_index_sequence<sizeof...(Ops)>{});
};

} // namespace koheron

#endif // __SERVER_CORE_DRIVER_ADAPTER_HPP__