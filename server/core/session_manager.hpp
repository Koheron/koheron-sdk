/// Sessions manager
///
/// (c) Koheron

#ifndef __SESSION_MANAGER_HPP__
#define __SESSION_MANAGER_HPP__

#include <map>
#include <vector>
#include <stack>
#include <memory>
#include <mutex>

#include "server_definitions.hpp"
#include "config.hpp"
#include "session_abstract.hpp"
#include "syslog.hpp"


namespace koheron {

// class SessionAbstract;
template<int socket_type> class Session;
class DriverManager;

class SessionManager
{
  public:
    SessionManager(DriverManager& drv_manager_, SysLog& syslog_);

    ~SessionManager();

    static int number_of_sessions;

    size_t get_number_of_sessions() const {return session_pool.size();}

    template<int socket_type>
    SessionID create_session(int comm_fd);

    std::vector<SessionID> get_session_ids();

    SessionAbstract& get_session(SessionID id) const {return *session_pool.at(id);}

    void delete_session(SessionID id);
    void delete_all();
    void exit_comm();

    DriverManager& driver_manager;
    SysLog& syslog;

  private:
    // Sessions pool
    std::map<SessionID, std::unique_ptr<SessionAbstract>> session_pool;
    std::vector<SessionID> reusable_ids;

    bool is_reusable_id(SessionID id);
    bool is_id_in_session_ids(SessionID id);

    std::mutex mutex;
};

template<int socket_type>
SessionID SessionManager::create_session(int comm_fd)
{
    std::lock_guard<std::mutex> lock(mutex);

    SessionID new_id;

    // Choose a reusable ID if available else
    // create a new ID equal to the session number
    if (reusable_ids.empty()) {
        new_id = number_of_sessions;
    } else {
        new_id = reusable_ids.back();
        reusable_ids.pop_back();
    }

    auto session = std::make_unique<Session<socket_type>>(comm_fd, new_id, syslog, driver_manager);

    session_pool.insert(std::pair<SessionID, std::unique_ptr<SessionAbstract>>(new_id,
                static_cast<std::unique_ptr<SessionAbstract>>(std::move(session))));
    number_of_sessions++;
    return new_id;
}

} // namespace koheron

#endif //__SESSION_MANAGER_HPP__
