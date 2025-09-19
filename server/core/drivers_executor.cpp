// driver_executor.cpp
#include "drivers_executor.hpp"
#include "drivers_manager.hpp"
#include "drivers_config.hpp"
#include "lib/meta_utils.hpp"
#include "services.hpp"
#include "commands.hpp"
#include "server.hpp"
#include <interface_drivers.hpp>
#include <cassert>

namespace koheron {

struct DriverExecutor::Impl {
    std::array<std::unique_ptr<DriverAbstract>, drivers::table::size - drivers::table::offset> wrappers{};

    template<driver_id id>
    void ensure_wrapper() {
        auto& slot = wrappers[id - 2];

        if (slot) {
            return;
        }

        auto& dm = services::require<DriverManager>();
        dm.ensure_core_started(id);
        slot = std::make_unique<Driver<id>>(dm.core<id>());
    }

    void ensure_wrapper_runtime(driver_id id) {
        auto seq = make_index_sequence_in_range<drivers::table::offset, drivers::table::size>();
        auto do_ensure = [this, id]<driver_id... ids>(std::index_sequence<ids...>) {
            ((id == ids ? (ensure_wrapper<ids>(), void()) : void()), ...);
        };
        do_ensure(seq);
    }

    template<driver_id... ids>
    int exec_on(DriverAbstract* abs, Command& cmd, std::index_sequence<ids...>) {
        int result = 0;
        bool done = false;
        (void)std::initializer_list<int>{
            ((abs->type == ids && !done)
                 ? (result = static_cast<Driver<ids>*>(abs)->execute(cmd), done = true, 0)
                 : 0)...};
        return result;
    }

    int execute(Command& cmd) {
        assert(cmd.driver < drivers::table::size);
        if (cmd.driver == 0) { // NoDriver
            return 0;
        }

        if (cmd.driver == 1) { // Server
            return services::require<Server>().execute(cmd);
        }

        ensure_wrapper_runtime(cmd.driver);
        auto* abs = wrappers[cmd.driver - drivers::table::offset].get();
        auto seq = make_index_sequence_in_range<drivers::table::offset, drivers::table::size>();
        return exec_on(abs, cmd, seq);
    }
};

DriverExecutor::DriverExecutor()
    : p_(std::make_unique<Impl>()) {}

DriverExecutor::~DriverExecutor() = default;
DriverExecutor::DriverExecutor(DriverExecutor&&) noexcept = default;
DriverExecutor& DriverExecutor::operator=(DriverExecutor&&) noexcept = default;

int DriverExecutor::execute(Command& cmd) {
    return p_->execute(cmd);
}

} // namespace koheron
