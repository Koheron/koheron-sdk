/// Sessions manager
///
/// (c) Koheron

#ifndef __SESSION_MANAGER_HPP__
#define __SESSION_MANAGER_HPP__

#include "server/network/configs/server_definitions.hpp"
#include "server/network/configs/config.hpp"
#include "server/network/session.hpp"

#include <map>
#include <vector>
#include <stack>
#include <memory>
#include <mutex>
#include <utility>
#include <string>
#include <filesystem>

namespace net {

template<int socket_type> class SocketSession;

class SessionManager
{
  public:
    SessionManager();

    ~SessionManager();

    static int number_of_sessions;

    size_t get_number_of_sessions() const;

    template<int socket_type>
    SessionID create_session(int comm_fd);

    std::vector<SessionID> get_session_ids();

    std::shared_ptr<Session> get_session_shared(SessionID id) const;

    void delete_session(SessionID id);
    void delete_all();
    void exit_comm();

    bool dump_rates(const std::filesystem::path& path);

  private:
    // Sessions pool
    std::map<SessionID, std::shared_ptr<Session>> session_pool;
    std::vector<SessionID> reusable_ids;

    bool is_reusable_id(SessionID id);
    bool is_id_in_session_ids(SessionID id);

    mutable std::mutex mutex;
};

template<int socket_type>
SessionID SessionManager::create_session(int comm_fd) {
    std::lock_guard lock(mutex);

    SessionID new_id;

    // Choose a reusable ID if available else
    // create a new ID equal to the session number
    if (reusable_ids.empty()) {
        new_id = number_of_sessions;
    } else {
        new_id = reusable_ids.back();
        reusable_ids.pop_back();
    }

    auto session = std::make_shared<SocketSession<socket_type>>(comm_fd, new_id);

    session_pool.emplace(new_id, std::move(session));
    number_of_sessions++;
    return new_id;
}

} // namespace net

#endif //__SESSION_MANAGER_HPP__
