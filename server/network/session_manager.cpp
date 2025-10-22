/// Implementation of session_manager.hpp
///
/// (c) Koheron

#include "server/network/session_manager.hpp"
#include "server/network/socket_session.hpp"

#include <cassert>
#include <thread>
#include <mutex>
#include <cstdio>
#include <string>
#include <vector>
#include <chrono>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <sys/socket.h>

namespace net {

SessionManager::SessionManager()
: session_pool(),
  reusable_ids(0)
{}

SessionManager::~SessionManager() {delete_all();}

int SessionManager::number_of_sessions = 0;

bool SessionManager::is_reusable_id(SessionID id) {
    for (auto& reusable_id : reusable_ids)
        if (reusable_id == id)
            return true;

    return false;
}

bool SessionManager::is_id_in_session_ids(SessionID id) {
    auto curr_ids = get_session_ids();

    for (auto& curr_id : curr_ids)
        if (curr_id == id)
            return true;

    return false;
}

std::vector<SessionID> SessionManager::get_session_ids() {
    std::vector<SessionID> res(0);

    for (auto& session : session_pool) {
        assert(!is_reusable_id(session.first));
        res.push_back(session.first);
    }

    return res;
}

void SessionManager::delete_session(SessionID id) {
    std::lock_guard lock(mutex);

    if (!is_id_in_session_ids(id)) {
        logf("Not allocated session ID: {}\n", id);
        return;
    }

    if (session_pool[id] != nullptr) {
        session_pool[id]->shutdown();
    }

    session_pool.erase(id);
    reusable_ids.push_back(id);
    number_of_sessions--;
}

void SessionManager::delete_all() {
    log("Closing all active sessions ...\n");
    assert(number_of_sessions == session_pool.size());

    if (!session_pool.empty()) {
        auto ids = get_session_ids();

        for (auto& id : ids) {
            logf("Delete session {}\n", id);
            delete_session(id);
        }
    }

    assert(number_of_sessions == 0);
}

void SessionManager::exit_comm() {
    for (auto& session : session_pool) {
        session.second->exit_comm();
    }
}

// ------------------------------------------------
// Dump data rates to JSON

inline bool atomic_write(const std::string& path, const std::string& data) {
    std::string tmp = path + ".tmp";
    {
        FILE* f = std::fopen(tmp.c_str(), "wb");
        if (!f) return false;
        size_t n = std::fwrite(data.data(), 1, data.size(), f);
        int e1 = std::fclose(f);
        if (n != data.size() || e1 != 0) { std::remove(tmp.c_str()); return false; }
    }
    // rename is atomic on POSIX when same filesystem
    if (std::rename(tmp.c_str(), path.c_str()) != 0) {
        std::remove(tmp.c_str());
        return false;
    }
    return true;
}

struct RateRow {
    SessionID id;
    std::string_view socket;
    int64_t total_rx;
    int64_t total_tx;
    double rx_mean, rx_win, rx_inst, rx_ewma, rx_max;
    double tx_mean, tx_win, tx_inst, tx_ewma, tx_max;
};

inline std::string json_escape(std::string_view s) {
    std::string out; out.reserve(s.size()+8);
    for (unsigned char c : s) {
        switch (c) {
            case '\"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20) { char buf[7]; std::snprintf(buf, sizeof(buf), "\\u%04x", c); out += buf; }
                else out += char(c);
        }
    }
    return out;
}

inline std::string build_json(const std::vector<RateRow>& rows) {
    // JSON object with timestamp and an array of sessions
    using clock = std::chrono::steady_clock;
    using sec   = std::chrono::duration<double>;
    const auto now  = clock::now().time_since_epoch();
    const auto secs = std::chrono::duration_cast<sec>(now).count();

    std::string j;
    j.reserve(1024 + rows.size() * 512);
    j += "{\"ts\":";           // steady_clock seconds (monotonic)
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.6f", secs);
    j += buf;
    j += ",\"sessions\":[";
    bool first = true;
    for (const auto& r : rows) {
        if (!first) j += ',';
        first = false;
        j += "{";
        j += "\"id\":" + std::to_string(r.id);
        j += ",\"socket\":\"" + json_escape(r.socket) + "\"";
        j += ",\"rx_total\":" + std::to_string(r.total_rx);
        j += ",\"tx_total\":" + std::to_string(r.total_tx);

        auto add_d = [&](const char* k, double v){
            j += ",\""; j += k; j += "\":";
            std::snprintf(buf, sizeof(buf), "%.3f", v);
            j += buf;
        };
        add_d("rx_mean", r.rx_mean);
        add_d("rx_win",  r.rx_win);
        add_d("rx_inst", r.rx_inst);
        add_d("rx_ewma", r.rx_ewma);
        add_d("rx_max",  r.rx_max);
        add_d("tx_mean", r.tx_mean);
        add_d("tx_win",  r.tx_win);
        add_d("tx_inst", r.tx_inst);
        add_d("tx_ewma", r.tx_ewma);
        add_d("tx_max",  r.tx_max);
        j += "}";
    }
    j += "]}";
    return j;
}

bool SessionManager::dump_rates(const std::string& out_path) {
    std::vector<RateRow> rows;
    rows.reserve(session_pool.size());

    for (const auto& [sid, session] : session_pool) {
        auto [rx, tx] = session->rates();

        rows.emplace_back(RateRow{
            .id       = sid,
            .socket   = listen_channel_desc[session->type],
            .total_rx = rx.total_bytes,
            .total_tx = tx.total_bytes,
            .rx_mean  = rx.mean_bps, .rx_win  = rx.window_bps, .rx_inst = rx.inst_bps,
            .rx_ewma  = rx.ewma_bps, .rx_max  = rx.max_bps,
            .tx_mean  = tx.mean_bps, .tx_win  = tx.window_bps, .tx_inst = tx.inst_bps,
            .tx_ewma  = tx.ewma_bps, .tx_max  = tx.max_bps
        });
    }

    return atomic_write(out_path, build_json(rows));
}

} // namespace net

