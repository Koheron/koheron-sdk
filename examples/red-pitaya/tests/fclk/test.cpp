// serverd_users.cpp
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>

#include "server/runtime/syslog.hpp"
#include "server/runtime/services.hpp"
#include "server/runtime/systemd.hpp"
#include "server/runtime/signal_handler.hpp"
#include "server/runtime/executor.hpp"

#include "server/network/listener_manager.hpp"
#include "server/network/commands.hpp"

using services::require;

// ---------------- Chat hub: queues per session + user directory ----------------
struct ChatHub {
    static constexpr size_t MAX_Q = 256;

    std::mutex m;
    std::unordered_map<uint32_t, std::deque<std::string>> queues;   // sid -> queue
    std::unordered_map<uint32_t, std::string> sid_to_user;           // sid -> user
    std::unordered_map<std::string, std::unordered_set<uint32_t>> user_to_sids; // user -> sids

    void ensure_queue(uint32_t sid) {
        (void)queues.emplace(sid, std::deque<std::string>{});
    }

    void register_user(uint32_t sid, std::string user) {
        std::lock_guard<std::mutex> L(m);
        ensure_queue(sid);
        if (auto it = sid_to_user.find(sid); it != sid_to_user.end()) {
            auto &oldset = user_to_sids[it->second];
            oldset.erase(sid);
            if (oldset.empty()) user_to_sids.erase(it->second);
        }
        sid_to_user[sid] = user;
        user_to_sids[user].insert(sid);
    }

    void push_sid(uint32_t sid, std::string msg) {
        std::lock_guard<std::mutex> L(m);
        ensure_queue(sid);
        auto& q = queues[sid];
        if (q.size() >= MAX_Q) q.pop_front();
        q.emplace_back(std::move(msg));
    }

    void push_user(const std::string& user, std::string msg) {
        std::lock_guard<std::mutex> L(m);
        auto uit = user_to_sids.find(user);
        if (uit == user_to_sids.end()) return;
        for (auto sid : uit->second) {
            ensure_queue(sid);
            auto& q = queues[sid];
            if (q.size() >= MAX_Q) q.pop_front();
            q.emplace_back(msg);
        }
    }

    void broadcast(std::string msg) {
        std::lock_guard<std::mutex> L(m);
        for (auto &kv : queues) {
            auto& q = kv.second;
            if (q.size() >= MAX_Q) q.pop_front();
            q.emplace_back(msg);
        }
    }

    std::optional<std::string> pop_one(uint32_t sid) {
        std::lock_guard<std::mutex> L(m);
        ensure_queue(sid);
        auto &q = queues[sid];
        if (q.empty()) return std::nullopt;
        std::string msg = std::move(q.front());
        q.pop_front();
        return msg;
    }

    std::string user_of(uint32_t sid) {
        std::lock_guard<std::mutex> L(m);
        auto it = sid_to_user.find(sid);
        return (it == sid_to_user.end()) ? std::string{} : it->second;
    }
};

static ChatHub g_chat;

static inline void drain_optional_payload(net::Command& cmd) {
    // Consume a possible string payload to keep framing aligned (length may be 0).
    std::string ignore; cmd.read_one(ignore);
}

static inline std::pair<std::string,std::string> split_first_line(const std::string& s) {
    auto pos = s.find('\n');
    if (pos == std::string::npos) return {s, std::string{}};
    return {s.substr(0, pos), s.substr(pos + 1)};
}

// ---------------- Executor ----------------
// Protocol (driver=0xFFFF):
//   0  : echo                 payload: <message>
//   11 : poll one             payload: (ignored; drained) -> "none" or message
//   20 : register username    payload: <username>   (creates queue)
//   21 : send to user         payload: "<target_username>\n<message>"
//   22 : broadcast            payload: "<message>"
class AppExecutor final : public rt::IExecutor {
  public:
    int handle_app(net::Command& cmd) override {
        if (cmd.driver != 0xFFFF) return 0;

        switch (cmd.operation) {
            case 0: { // echo
                std::string s; cmd.read_one(s);
                return cmd.send(s);
            }

            case 11: { // poll
                drain_optional_payload(cmd);
                if (auto m = g_chat.pop_one(cmd.session_id)) return cmd.send(*m);
                return cmd.send("none");
            }

            case 20: { // register username
                std::string user; cmd.read_one(user);
                if (user.empty()) return cmd.send("err: empty username");
                g_chat.register_user(cmd.session_id, user);
                return cmd.send("ok: user=" + user);
            }

            case 21: { // send to user: "<user>\n<message>"
                std::string pl; cmd.read_one(pl);
                auto [to_user, msg] = split_first_line(pl);
                if (to_user.empty()) return cmd.send("err: empty target");
                auto from = g_chat.user_of(cmd.session_id);
                if (from.empty()) from = "sid:" + std::to_string(cmd.session_id);
                std::string wrapped = "[dm] from " + from + ": " + msg;
                g_chat.push_user(to_user, std::move(wrapped));
                return cmd.send("ok: sent to " + to_user);
            }

            case 22: { // broadcast: "<message>"
                std::string msg; cmd.read_one(msg);
                auto from = g_chat.user_of(cmd.session_id);
                if (from.empty()) from = "sid:" + std::to_string(cmd.session_id);
                g_chat.broadcast("[all] " + from + ": " + msg);
                return cmd.send("ok: broadcast");
            }

            default:
                // reply so clients never hang
                drain_optional_payload(cmd);
                return cmd.send("err: unsupported op " + std::to_string(cmd.operation));
        }
    }
};

// ---------------- Main ----------------
int main() {
    rt::SignalHandler signal_handler;
    if (signal_handler.init() < 0) std::exit(EXIT_FAILURE);

    rt::provide_executor<AppExecutor>();

    net::ListenerManager lm;
    if (lm.start() < 0) std::exit(EXIT_FAILURE);

    bool ready_notified = false;
    for (;;) {
        if (!ready_notified && lm.is_ready()) {
            log("chat app is ready\n");
            if constexpr (net::config::notify_systemd) {
                rt::systemd::notify_ready("chat app is ready");
            }
            ready_notified = true;
        }
        if (signal_handler.interrupt()) {
            log("Interrupt received, shutting down...\n");
            lm.shutdown();
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return 0;
}
