/// (c) Koheron

#ifndef __KOHERON_SESSION_HPP__
#define __KOHERON_SESSION_HPP__

#include <string>
#include <ctime>
#include <vector>
#include <array>
#include <memory>
#include <unistd.h>
#include <type_traits>
#include <cassert>

#include "commands.hpp"
#include "serializer_deserializer.hpp"
#include "server_definitions.hpp"
#include "syslog.hpp"
#include "session_abstract.hpp"
#include "drivers_manager.hpp"
#include "websocket.hpp"

namespace koheron {

/// Session
///
/// Receive and parse the client request for execution.
///
/// By calling the appropriate socket interface, it offers
/// an abstract communication layer to the drivers. Thus
/// shielding them from the underlying communication protocol.
template<int socket_type>
class Session : public SessionAbstract
{
  public:
    Session(int comm_fd, SessionID id_, SysLog& syslog_, DriverManager& drv_manager_);

    int run();

    SessionID get_id() const {return id;}

    // Receive - Send

    // TODO Move in Session<TCP> specialization
    int64_t rcv_n_bytes(char *buffer, int64_t n_bytes);

    template<typename... Tp> std::tuple<int, Tp...> deserialize(Command& cmd, std::false_type);
    template<typename... Tp> std::tuple<int, Tp...> deserialize(Command& cmd, std::true_type);

    // The command is passed in argument since for the WebSocket the vector data
    // are stored into it. This implies that the whole vector is already stored on
    // the stack which might not be a good thing.
    //
    // TCP sockets won't use it as it reads directly the TCP buffer.
    template<typename Tp>
    int recv(Tp& container, Command& cmd);

    template<typename T, size_t N>
    int recv(std::array<T, N>& arr, Command&);

    template<typename T>
    int recv(std::vector<T>& vec, Command&);

    template<uint16_t class_id, uint16_t func_id, typename... Args>
    int send(Args&&... args) {
        dynamic_serializer.build_command<class_id, func_id>(send_buffer, std::forward<Args>(args)...);
        const auto bytes_send = write(send_buffer.data(), send_buffer.size());

        if (bytes_send == 0) {
            status = CLOSED;
        }

        return bytes_send;
    }

  private:
    int comm_fd;  ///< Socket file descriptor
    SessionID id;
    SysLog& syslog;
    DriverManager& driver_manager;

    struct EmptyBuffer {};
    std::conditional_t<socket_type == TCP || socket_type == UNIX,
                       Buffer<KOHERON_RECV_DATA_BUFF_LEN>, EmptyBuffer> recv_data_buff;

    struct EmptyWebsock {
        EmptyWebsock(SysLog&) {}
    };

    std::conditional_t<socket_type == WEBSOCK, WebSocket, EmptyWebsock> websock;

    std::vector<unsigned char> send_buffer;
    DynamicSerializer<1024> dynamic_serializer;

    enum {CLOSED, OPENED};
    int status;

  private:
    int init_socket();
    int exit_socket();

    int init_session() {
        return init_socket();
    }

    void exit_session() {
        if (exit_socket() < 0) {
            syslog.print<WARNING>("An error occured during session exit\n");
        }
    }

    int read_command(Command& cmd);

    int64_t get_pack_length() {
        Buffer<sizeof(uint64_t)> buff;
        const auto err = rcv_n_bytes(buff.data(), sizeof(uint32_t));

        if (err < 0) {
            syslog.print<ERROR>("Cannot read pack length\n");
            return -1;
        }

        return std::get<0>(buff.deserialize<uint32_t>());
    }

    template<class T> int write(const T *data, unsigned int len);

friend class SessionManager;
};

template<int socket_type>
Session<socket_type>::Session(int comm_fd_, SessionID id_, SysLog& syslog_, DriverManager& drv_manager_)
: SessionAbstract(socket_type)
, comm_fd(comm_fd_)
, id(id_)
, syslog(syslog_)
, driver_manager(drv_manager_)
, websock(syslog)
, send_buffer(0)
, status(OPENED)
{}

template<int socket_type>
int Session<socket_type>::run()
{
    if (init_session() < 0) {
        return -1;
    }

    while (!exit_signal) {
        Command cmd;
        const int nb_bytes_rcvd = read_command(cmd);

        if (exit_signal) {
            break;
        }

        if (nb_bytes_rcvd <= 0) {
            // We don't call exit_session() here because the socket is already closed.
            return nb_bytes_rcvd;
        }

        if (driver_manager.execute(cmd) < 0) {
            syslog.print<ERROR>("Failed to execute command [driver = %i, operation = %i]\n", cmd.driver, cmd.operation);
        }

        if (status == CLOSED) {
            break;
        }
    }

    exit_session();
    return 0;
}

// -----------------------------------------------
// TCP
// -----------------------------------------------

template<>
int64_t Session<TCP>::rcv_n_bytes(char *buffer, int64_t n_bytes);

template<>
template<typename T, size_t N>
inline int Session<TCP>::recv(std::array<T, N>& arr, Command&)
{
    return rcv_n_bytes(reinterpret_cast<char*>(arr.data()), size_of<T, N>);
}

template<>
template<typename T>
inline int Session<TCP>::recv(std::vector<T>& vec, Command&)
{
    const auto length = get_pack_length() / sizeof(T);

    if (length < 0) {
        return -1;
    }

    vec.resize(length);
    const auto err = rcv_n_bytes(reinterpret_cast<char *>(vec.data()), length * sizeof(T));

    if (err >= 0) {
        syslog.print<DEBUG>("TCPSocket: Received a vector of %lu bytes\n", length);
    }

    return err;
}

template<>
template<>
inline int Session<TCP>::recv(std::string& str, Command&)
{
    const auto length = get_pack_length();

    if (length < 0) {
        return -1;
    }

    str.resize(length);
    const auto err = rcv_n_bytes(str.data(), length);

    if (err >= 0) {
       syslog.print<DEBUG>("TCPSocket: Received a string of %lu bytes\n", length);
    }

    return err;
}

template<>
template<typename... Tp>
inline std::tuple<int, Tp...> Session<TCP>::deserialize(Command&, std::false_type)
{
    return std::make_tuple(0);
}

template<>
template<typename... Tp>
inline std::tuple<int, Tp...> Session<TCP>::deserialize(Command&, std::true_type)
{
    constexpr auto pack_len = required_buffer_size<Tp...>();
    Buffer<pack_len> buff;
    const int err = rcv_n_bytes(buff.data(), pack_len);
    return std::tuple_cat(std::make_tuple(err), buff.template deserialize<Tp...>());
}

template<>
template<class T>
inline int Session<TCP>::write(const T *data, unsigned int len)
{
    const auto bytes_send = sizeof(T) * len;
    const int n_bytes_send = ::write(comm_fd, static_cast<const unsigned char*>(data), bytes_send);

    if (n_bytes_send == 0) {
       syslog.print<ERROR>("TCPSocket::write: Connection closed by client\n");
       return 0;
    }

    if (n_bytes_send < 0) {
       syslog.print<ERROR>("TCPSocket::write: Can't write to client\n");
       return -1;
    }

    if (n_bytes_send != static_cast<int>(bytes_send)) {
        syslog.print<ERROR>("TCPSocket::write: Some bytes have not been sent\n");
        return -1;
    }

    syslog.print<DEBUG>("[S] [%u bytes]\n", bytes_send);
    return static_cast<int>(bytes_send);
}

// -----------------------------------------------
// Unix socket
// -----------------------------------------------

// Unix socket has the same interface than TCP socket
template<>
class Session<UNIX> : public Session<TCP>
{
  public:
    Session<UNIX>(int comm_fd_, SessionID id_, SysLog& syslog_, DriverManager& drv_manager_)
    : Session<TCP>(comm_fd_, id_, syslog_, drv_manager_) {}
};

// -----------------------------------------------
// WebSocket
// -----------------------------------------------

template<>
template<typename T, size_t N>
inline int Session<WEBSOCK>::recv(std::array<T, N>& arr, Command& cmd)
{
    arr = cmd.payload.extract_array<T, N>();
    return 0;
}

template<>
template<typename T>
inline int Session<WEBSOCK>::recv(std::vector<T>& vec, Command& cmd)
{
    const auto length = std::get<0>(cmd.payload.deserialize<uint32_t>());

    if (length > CMD_PAYLOAD_BUFFER_LEN) {
        syslog.print<ERROR>("WebSocket: Payload size overflow during buffer reception\n");
        return -1;
    }

    cmd.payload.to_vector(vec, length / sizeof(T));
    return 0;
}

template<>
template<>
inline int Session<WEBSOCK>::recv(std::string& str, Command& cmd)
{
    const auto length = std::get<0>(cmd.payload.deserialize<uint32_t>());

    if (length > CMD_PAYLOAD_BUFFER_LEN) {
        syslog.print<ERROR>("WebSocket::rcv_vector: Payload size overflow\n");
        return -1;
    }

    cmd.payload.to_string(str, length);
    return 0;
}

template<>
template<typename... Tp>
inline std::tuple<int, Tp...> Session<WEBSOCK>::deserialize(Command& cmd, std::false_type)
{
    return std::make_tuple(0);
}

template<>
template<typename... Tp>
inline std::tuple<int, Tp...> Session<WEBSOCK>::deserialize(Command& cmd, std::true_type)
{
    return std::tuple_cat(std::make_tuple(0), cmd.payload.deserialize<Tp...>());
}

template<>
template<class T>
inline int Session<WEBSOCK>::write(const T *data, unsigned int len)
{
    return websock.send(data, len);
}


// -----------------------------------------------
// Select session type
// -----------------------------------------------

// For the template in the middle, see:
// http://stackoverflow.com/questions/1682844/templates-template-function-not-playing-well-with-classs-template-member-funct/1682885#1682885

template<typename... Tp>
inline std::tuple<int, Tp...> SessionAbstract::deserialize(Command& cmd)
{
    switch (this->type) {
        case TCP:
            return static_cast<Session<TCP>*>(this)->template deserialize<Tp...>(cmd, std::integral_constant<bool, 0 < sizeof...(Tp)>());
        case UNIX:
            return static_cast<Session<UNIX>*>(this)->template deserialize<Tp...>(cmd, std::integral_constant<bool, 0 < sizeof...(Tp)>());
        case WEBSOCK:
            return static_cast<Session<WEBSOCK>*>(this)->template deserialize<Tp...>(cmd, std::integral_constant<bool, 0 < sizeof...(Tp)>());
        default:
            return std::tuple_cat(std::make_tuple(-1), std::tuple<Tp...>());
    }
}

template<typename Tp>
inline int SessionAbstract::recv(Tp& container, Command& cmd)
{
    switch (this->type) {
        case TCP:
            return static_cast<Session<TCP>*>(this)->recv(container, cmd);
        case UNIX:
            return static_cast<Session<UNIX>*>(this)->recv(container, cmd);
        case WEBSOCK:
            return static_cast<Session<WEBSOCK>*>(this)->recv(container, cmd);
        default:
            return -1;
    }
}

template<uint16_t class_id, uint16_t func_id, typename... Args>
inline int SessionAbstract::send(Args&&... args)
{
    switch (this->type) {
        case TCP:
            return static_cast<Session<TCP>*>(this)->template send<class_id, func_id>(std::forward<Args>(args)...);
        case UNIX:
            return static_cast<Session<UNIX>*>(this)->template send<class_id, func_id>(std::forward<Args>(args)...);
        case WEBSOCK:
            return static_cast<Session<WEBSOCK>*>(this)->template send<class_id, func_id>(std::forward<Args>(args)...);
        default:
            return -1;
    }
}

// Cast abstract session unique_ptr
template<int socket_type>
Session<socket_type>*
cast_to_session(const std::unique_ptr<SessionAbstract>& sess_abstract)
{
    return static_cast<Session<socket_type>*>(sess_abstract.get());
}

} // namespace koheron

#endif // __KOHERON_SESSION_HPP__
