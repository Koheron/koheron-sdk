/// Implementation of session_manager.hpp
///
/// (c) Koheron

#include "server/network/session_manager.hpp"
#include "server/network/socket_session.hpp"
#include "server/utilities/rate_tracker.hpp"

#include <cassert>
#include <thread>
#include <mutex>
#include <cstdio>
#include <string>
#include <vector>

namespace net {

SessionManager::SessionManager()
: session_pool(),
  reusable_ids(0)
{}

SessionManager::~SessionManager() {delete_all();}

int SessionManager::number_of_sessions = 0;

size_t SessionManager::get_number_of_sessions() const {
    std::lock_guard lock(mutex);
    return session_pool.size();
}

std::shared_ptr<Session> SessionManager::get_session_shared(SessionID id) const {
    std::lock_guard lock(mutex);
    return session_pool.at(id);
}

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
    std::lock_guard lock(mutex);
    std::vector<SessionID> res(0);

    for (auto& session : session_pool) {
        assert(!is_reusable_id(session.first));
        res.push_back(session.first);
    }

    return res;
}

void SessionManager::delete_session(SessionID id) {
    std::shared_ptr<Session> session;

    {
        std::lock_guard lock(mutex);
        auto it = session_pool.find(id);

        if (it == session_pool.end()) {
            logf("Not allocated session ID: {}\n", id);
            return;
        }

        session = std::move(it->second);
        session_pool.erase(it);
        reusable_ids.push_back(id);
        number_of_sessions--;
    }

    if (session != nullptr) {
        session->shutdown();
    }
}

void SessionManager::delete_all() {
    log("Closing all active sessions ...\n");
    auto ids = get_session_ids();

    if (!ids.empty()) {

        for (auto& id : ids) {
            logf("Delete session {}\n", id);
            delete_session(id);
        }
    }

    assert(get_number_of_sessions() == 0);
}

void SessionManager::exit_comm() {
    std::lock_guard lock(mutex);

    for (auto& session : session_pool) {
        session.second->exit_comm();
    }
}

// ------------------------------------------------
// Dump data rates to JSON

bool SessionManager::dump_rates(const std::filesystem::path& path) {
    std::vector<ut::RateRow> rows;

    {
        std::lock_guard lock(mutex);
        rows.reserve(session_pool.size());

        for (const auto& [sid, session] : session_pool) {
            auto [rx, tx] = session->rates();

            rows.emplace_back(ut::RateRow{
                .id       = sid,
                .name     = listen_channel_desc[session->type],
                .total_rx = rx.total_bytes,
                .total_tx = tx.total_bytes,
                .rx_mean  = rx.mean_bps, .rx_win  = rx.window_bps, .rx_inst = rx.inst_bps,
                .rx_ewma  = rx.ewma_bps, .rx_max  = rx.max_bps,
                .tx_mean  = tx.mean_bps, .tx_win  = tx.window_bps, .tx_inst = tx.inst_bps,
                .tx_ewma  = tx.ewma_bps, .tx_max  = tx.max_bps
            });
        }
    }

    return ut::dump_rates_to_json(path, "sessions", rows);
}

} // namespace net

