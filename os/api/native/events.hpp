#pragma once
#include "common.hpp"
#include <microhttpd.h>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

namespace koheron::management {
// Read-only RFC6455 stream. Commands remain HTTP requests with explicit results.
class EventHub {
    struct Client {
        MHD_socket socket;
        MHD_UpgradeResponseHandle* handle;
        std::string input, output;
        bool closing = false;
        std::chrono::steady_clock::time_point deadline{};
    };
    std::mutex mutex_;
    std::condition_variable changed_;
    std::vector<Client> clients_;
    bool stopped_ = false, dirty_ = true;
    std::function<std::string()> snapshot_;
    std::thread thread_;
    void run();
public:
    explicit EventHub(std::function<std::string()> snapshot);
    ~EventHub();
    void wake();
    void stop();
    void accept(MHD_socket socket, MHD_UpgradeResponseHandle* handle, std::string_view input);
    [[nodiscard]] MHD_Result upgrade(MHD_Connection* connection);
};
}
