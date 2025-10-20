#ifndef __SERVER_CORE_DRIVER_ADAPTER_HPP__
#define __SERVER_CORE_DRIVER_ADAPTER_HPP__

#include "server/core/drivers/driver.hpp"
#include "server/core/commands.hpp"
#include "server/core/session.hpp"
#include "server/utilities/meta_utils.hpp"

#include <array>
#include <type_traits>
#include <utility>
#include <mutex>
#include <tuple>
#include <vector>
#include <string>
#include <string_view>
#include <ranges>

namespace koheron {

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
        if (!cmd.read_arguments(args, IS{})) {
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

template<std::size_t N>
struct FixedString {
    std::array<char, N> data{};
    // includes the terminating '\0'
    constexpr FixedString(const char (&str)[N]) {
        for (std::size_t i = 0; i < N; ++i) data[i] = str[i];
    }
    constexpr std::string_view sv() const { return {data.data(), N - 1}; }

    static constexpr std::size_t size = N;

    // pointer to storage (N includes the trailing '\0' in your examples)
    constexpr const char* c_str() const noexcept { return data.data(); }

    // string_view without the trailing '\0'
    constexpr std::string_view view() const noexcept {
        std::size_t len = 0;
        while (len < N && data[len] != '\0') ++len;
        return std::string_view(data.data(), len);
    }
};

// deduction guide
template<std::size_t N>
FixedString(const char (&)[N]) -> FixedString<N>;

template<auto PMF, FixedString... ArgNames>
struct Op {
    static constexpr auto pmf = PMF;

    using traits    = pmf_traits<decltype(PMF)>;
    using args_tuple= typename traits::args;
    static constexpr std::size_t arity = std::tuple_size_v<args_tuple>;

    // If names are provided, they must match arity:
    static_assert(sizeof...(ArgNames) == 0 || sizeof...(ArgNames) == arity,
                  "Number of parameter names must match function arity");

    // Return arg names as std::array<std::string_view, arity>.
    // If none provided, return empty views (you can fill defaults later when emitting JSON).
    static constexpr auto arg_names() {
        std::array<std::string_view, arity> a{};
        if constexpr (sizeof...(ArgNames) > 0) {
            std::size_t i = 0;
            // fold to assign
            ((a[i++] = ArgNames.sv()), ...);
        }
        return a;
    }
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