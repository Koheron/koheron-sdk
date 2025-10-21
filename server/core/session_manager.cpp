/// Implementation of session_manager.hpp
///
/// (c) Koheron

#include "server/core/session_manager.hpp"
#include "server/core/socket_session.hpp"

#include <cassert>
#include <sys/socket.h>
#include <thread>
#include <mutex>

namespace koheron {

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

} // namespace koheron

