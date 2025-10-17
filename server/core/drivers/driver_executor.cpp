// driver_executor.cpp

#include "server/core/drivers/driver_executor.hpp"
#include "server/core/commands.hpp"
#include "server/core/session_manager.hpp"
#include "server/core/drivers/drivers_config.hpp"

#include "server/utilities/meta_utils.hpp"
#include "server/runtime/services.hpp"
#include "server/runtime/driver_manager.hpp"

#include <interface_drivers.hpp>
#include <drivers_json.hpp>

#include <cassert>

namespace koheron {

/// Operations associated to the Server "driver"
enum Operation {
    GET_VERSION = 0,            ///< Send the version of the server
    GET_CMDS = 1,               ///< Send the commands numbers
    server_op_num,
};

struct DriverExecutor::Impl {
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
            auto& session = services::require<SessionManager>().get_session(cmd.session_id);

            switch (cmd.operation) {
            case GET_VERSION:
                return session.send<1, GET_VERSION>(KOHERON_VERSION);
            case GET_CMDS:
                return session.send<1, GET_CMDS>(build_drivers_json());
            case server_op_num:
            default:
                log<ERROR>("Server::execute unknown operation\n");
                return -1;
            }
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
