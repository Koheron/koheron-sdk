#ifndef __SERVER_CORE_DRIVER_ADAPTER_HPP__
#define __SERVER_CORE_DRIVER_ADAPTER_HPP__

#include "server/executor/driver.hpp"
#include "server/network/commands.hpp"
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

template<class C, auto PMF>
struct OpThunk {
    static int call(C& obj, net::Command& cmd) {
        return cmd.op_invoke(obj, PMF);
    }
};

template<int DriverID, class C, class... Ops>
class DriverAdapter : public DriverAbstract {
  public:
    explicit DriverAdapter(C& impl) : DriverAbstract(DriverID), obj(impl) {}

    int execute(net::Command& cmd) {
        std::lock_guard lock(mutex_);
        const int op = static_cast<int>(cmd.operation);

        if (op < 0 || op >= static_cast<int>(table_.size())) {
            return -1;
        }

        auto* fn = table_[op];
        return fn ? fn(obj, cmd) : -1;
    }

  private:
    C& obj;
    std::mutex mutex_;

    using Entry = int(*)(C&, net::Command&);

    template<std::size_t I>
    static constexpr int op_id_for() {
        return static_cast<int>(I);     // position == id
    }

    template<std::size_t... I>
    static constexpr int max_id(std::index_sequence<I...>) {
        int m = -1;
        ((m = (op_id_for<I>() > m ? op_id_for<I>() : m)), ...);
        return m;
    }

    template<std::size_t... I>
    static constexpr auto make_table(std::index_sequence<I...>) {
        // size is max_id + 1; default-init to nullptrs (OK in constexpr)
        constexpr int N = max_id(std::index_sequence<I...>{}) + 1;
        std::array<Entry, N> a{}; // all nullptr

        // Assign each slot via a fold; no loops -> constexpr friendly.
        (( a[ op_id_for<I>() ] =
              &OpThunk<C, Ops::pmf>::call ), ...);

        return a;
    }

    static constexpr auto table_ =
        make_table(std::make_index_sequence<sizeof...(Ops)>{});
};

} // namespace koheron

#endif // __SERVER_CORE_DRIVER_ADAPTER_HPP__
