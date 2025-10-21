#ifndef __SERVER_RUNTIME_EXECUTOR_HPP__
#define __SERVER_RUNTIME_EXECUTOR_HPP__

#include "server/runtime/services.hpp"

#include <memory>
#include <string>
#include <type_traits>
#include <string_view>
#include <span>

namespace net { class Command; }

namespace rt {

enum : uint16_t { DRIVER_NONE = 0, DRIVER_SERVER = 1 };
enum : uint16_t { GET_VERSION = 0, GET_CMDS = 1 };

struct IExecutor {
    virtual ~IExecutor() = default;
    int execute(net::Command& cmd);

    // App-specific handling (cmd.driver > 1)
    virtual int handle_app(net::Command& cmd) = 0;

    virtual std::string server_version() const { return "1.0"; }
    virtual std::string_view drivers_json() const { return drivers_json_storage_; }

    void set_drivers_json(std::string json) { drivers_json_storage_ = std::move(json); }

  private:
    std::string drivers_json_storage_;
};

inline std::string build_drivers_json_from_chunks(std::span<const std::string_view> chunks) {
    std::string out;
    out += "[";
    out += R"({"class":"KServer","id":1,"functions":[)"
           R"({"name":"get_version","id":0,"args":[],"ret_type":"const char *"},)"
           R"({"name":"get_cmds","id":1,"args":[],"ret_type":"std::string"}]})";
    for (std::string_view c : chunks) {
        out += ",";
        out += c;
    }
    out += "]";
    return out;
}

template<class Exec, class... Args>
requires std::derived_from<Exec, IExecutor>
inline std::shared_ptr<IExecutor> provide_executor(Args&&... args) {
    auto p = std::make_shared<Exec>(std::forward<Args>(args)...);
    services::provide<IExecutor>(p);
    return p;
}

} // namespace rt

#endif // __SERVER_RUNTIME_EXECUTOR_HPP__