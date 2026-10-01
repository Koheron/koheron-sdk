#include "server/executor/executor.hpp"
#include "server/executor/drivers_config.hpp"
#include "server/network/commands.hpp"
#include "server/utilities/meta_utils.hpp"
#include "server/runtime/services.hpp"
#include "server/runtime/driver_manager.hpp"

#include <interface_drivers.hpp>
#include <drivers_json.hpp>

#include <cassert>

namespace koheron {

struct Executor::Impl {
    std::array<std::unique_ptr<DriverAbstract>, drivers::table::size - drivers::table::offset> wrappers{};

    template<driver_id id>
    void ensure_wrapper() {
        auto& slot = wrappers[id - 2];

        if (slot) {
            return;
        }

        auto& dm = services::require<rt::DriverManager>();
        slot = std::make_unique<Driver<id>>(dm.get<id>());
    }

    void ensure_wrapper_runtime(driver_id id) {
        auto seq = make_index_sequence_in_range<drivers::table::offset, drivers::table::size>();
        auto do_ensure = [this, id]<driver_id... ids>(std::index_sequence<ids...>) {
            ((id == ids ? (ensure_wrapper<ids>(), void()) : void()), ...);
        };
        do_ensure(seq);
    }

    template<driver_id... ids>
    int exec_on(DriverAbstract* abs, net::Command& cmd, std::index_sequence<ids...>) {
        int result = 0;
        bool done = false;
        (void)std::initializer_list<int>{
            ((abs->type == ids && !done)
                 ? (result = static_cast<Driver<ids>*>(abs)->execute(cmd), done = true, 0)
                 : 0)...};
        return result;
    }

    int execute(net::Command& cmd) {
        assert(cmd.driver < drivers::table::size);
        ensure_wrapper_runtime(cmd.driver);
        auto* abs = wrappers[cmd.driver - drivers::table::offset].get();
        auto seq = make_index_sequence_in_range<drivers::table::offset, drivers::table::size>();
        return exec_on(abs, cmd, seq);
    }
};

Executor::Executor()
    : p_(std::make_unique<Impl>()) {
    set_drivers_json(build_drivers_json());
}

Executor::~Executor() = default;
Executor::Executor(Executor&&) noexcept = default;
Executor& Executor::operator=(Executor&&) noexcept = default;

int Executor::handle_app(net::Command& cmd) {
    return p_->execute(cmd);
}

} // namespace koheron
