/// (c) Koheron

#ifndef __KOHERON_SESSION_HPP__
#define __KOHERON_SESSION_HPP__

#include "server/runtime/syslog.hpp"
#include "server/runtime/services.hpp"
#include "server/utilities/concepts.hpp"

#include "server/core/configs/server_definitions.hpp"
#include "server/core/commands.hpp"
#include "server/core/serializer_deserializer.hpp"
#include "server/core/session_abstract.hpp"
#include "server/core/drivers/driver_executor.hpp"
#include "server/core/websocket.hpp"

#include <string>
#include <vector>
#include <array>
#include <memory>
#include <unistd.h>
#include <type_traits>
#include <cassert>
#include <memory_resource>
#include <ranges>
#include <span>
#include <cstddef>
#include <cerrno>

namespace koheron {

/// Session
///
/// Receive and parse the client request for execution
template<int socket_type>
class Session : public SessionAbstract
{
  public:
    Session(int comm_fd, SessionID id_);

    int run();

    SessionID get_id() const {return id;}

    // Receive - Send
    template<typename... Tp>
    std::tuple<int, Tp...> deserialize([[maybe_unused]] Buffer<CMD_PAYLOAD_BUFFER_LEN>& payload);

    // The command is passed in argument since for the WebSocket the vector data
    // are stored into it. This implies that the whole vector is already stored on
    // the stack which might not be a good thing.
    //
    // TCP sockets won't use it as it reads directly the TCP buffer.
    template<resizableContiguousRange R>
    int recv(R& container, Buffer<CMD_PAYLOAD_BUFFER_LEN>& payload);

    template<uint16_t class_id, uint16_t func_id, typename... Args>
    int send(Args&&... args) {
        builder.reset_into(send_buffer);
        builder.write_header<class_id, func_id>();
        builder.push(std::forward<Args>(args)...);
        const auto bytes_send = write(send_buffer);

        if (bytes_send == 0) {
            status = CLOSED;
        }

        return bytes_send;
    }

  private:
    int comm_fd;  ///< Socket file descriptor
    SessionID id;

    struct EmptyBuffer {};
    std::conditional_t<socket_type == TCP || socket_type == UNIX,
                       Buffer<KOHERON_RECV_DATA_BUFF_LEN>, EmptyBuffer> recv_data_buff;

    struct EmptyWebsock {
        EmptyWebsock() {}
    };

    std::conditional_t<socket_type == WEBSOCK, WebSocket, EmptyWebsock> websock;

    // First 4 KiB of allocations come from initial_storage, no heap at all
    // If we exceed it will grab memory from the system allocator.
    std::array<std::byte, 4096> seed{};
    std::pmr::monotonic_buffer_resource pool{
        seed.data(), seed.size(), std::pmr::get_default_resource()
    };
    std::pmr::vector<unsigned char> send_buffer{ &pool };
    CommandBuilder builder;

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
            log<WARNING>("An error occured during session exit\n");
        }
    }

    int read_command(Command& cmd);

    template<std::ranges::contiguous_range R>
    int write(const R& r);

friend class SessionManager;
};

template<int socket_type>
Session<socket_type>::Session(int comm_fd_, SessionID id_)
: SessionAbstract(socket_type)
, comm_fd(comm_fd_)
, id(id_)
, websock()
, send_buffer(0)
, status(OPENED)
{}

template<int socket_type>
int Session<socket_type>::run() {
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

        if (services::require<DriverExecutor>().execute(cmd) < 0) {
            logf<ERROR>("Failed to execute command [driver = {}, operation = {}]\n",
                        cmd.driver, cmd.operation);
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

// Reads until span is filled, returns bytes read (0 = EOF), or -1 on error.
inline ssize_t read_exact(int fd, std::span<std::byte> buf) {
    size_t done = 0;
    while (done < buf.size()) {
        ssize_t n = ::read(fd, buf.data() + done, buf.size() - done);

        if (n == 0) {
            log("TCPSocket: Connection closed by client\n");
            return 0;
        }

        if (n < 0) {
            if (errno == EINTR) { // interrupted -> retry
                continue;
            }

            logf<ERROR>("TCPSocket: Can't receive data (errno={})\n", errno);
            return -1;
        }

        done += static_cast<size_t>(n);
    }

    return static_cast<ssize_t>(done);
}

inline int64_t get_pack_length(int comm_fd) {
    Buffer<sizeof(uint64_t)> buff;
    const auto err = read_exact(
        comm_fd,
        std::as_writable_bytes(std::span{buff.data(), sizeof(uint32_t)})
    );

    if (err < 0) {
        log<ERROR>("Cannot read pack length\n");
        return -1;
    }

    return std::get<0>(buff.deserialize<uint32_t>());
}

template<>
template<resizableContiguousRange R>
int Session<TCP>::recv(R& c, Buffer<CMD_PAYLOAD_BUFFER_LEN>&) {
    using T = R::value_type;
    const auto length = get_pack_length(comm_fd) / sizeof(T);

    if (length < 0) {
        return -1;
    }

    c.resize(length);
    const auto err = read_exact(
        comm_fd,
        std::as_writable_bytes(std::span{c.data(), length})
    );

    if (err >= 0) {
        logf<DEBUG>("TCPSocket: Received a container of {} bytes\n", length);
    }

    return err;
}

template<>
template<typename... Tp>
std::tuple<int, Tp...> Session<TCP>::deserialize(Buffer<CMD_PAYLOAD_BUFFER_LEN>&) {
    if constexpr (sizeof...(Tp) == 0) {
        return std::tuple{0};
    } else {
        constexpr auto pack_len = required_buffer_size<Tp...>();
        Buffer<pack_len> buff;
        const auto err = read_exact(
            comm_fd,
            std::as_writable_bytes(std::span{buff.data(), pack_len})
        );
        return std::tuple_cat(std::tuple{err}, buff.template deserialize<Tp...>());
    }
}

template<>
template<std::ranges::contiguous_range R>
int Session<TCP>::write(const R& r) {
    using T = std::remove_cvref_t<std::ranges::range_value_t<R>>;

    const auto bytes_send = sizeof(T) * std::size(r);
    const int n_bytes_send = ::write(comm_fd, std::data(r), bytes_send);

    if (n_bytes_send == 0) {
       log<ERROR>("TCPSocket::write: Connection closed by client\n");
       return 0;
    }

    if (n_bytes_send < 0) {
       log<ERROR>("TCPSocket::write: Can't write to client\n");
       return -1;
    }

    if (n_bytes_send != static_cast<int>(bytes_send)) {
        log<ERROR>("TCPSocket::write: Some bytes have not been sent\n");
        return -1;
    }

    logf<DEBUG>("[S] [{} bytes]\n", bytes_send);
    return static_cast<int>(bytes_send);
}

// -----------------------------------------------
// Unix socket
// -----------------------------------------------

template<> class Session<UNIX> : public Session<TCP> {
    using Session<TCP>::Session;
};

// -----------------------------------------------
// WebSocket
// -----------------------------------------------

template<>
template<resizableContiguousRange R>
int Session<WEBSOCK>::recv(R& c, Buffer<CMD_PAYLOAD_BUFFER_LEN>& payload) {
    const auto [length] = payload.deserialize<uint32_t>();

    if (length > CMD_PAYLOAD_BUFFER_LEN) {
        log<ERROR>("WebSocket::rcv dynamic container: Payload size overflow\n");
        return -1;
    }

    payload.to_container(c, length);
    return 0;
}

template<>
template<typename... Tp>
std::tuple<int, Tp...> Session<WEBSOCK>::deserialize(Buffer<CMD_PAYLOAD_BUFFER_LEN>& payload) {
    if constexpr (sizeof...(Tp) == 0) {
        return std::tuple{0};
    } else {
        return std::tuple_cat(std::tuple{0}, payload.deserialize<Tp...>());
    }
}

template<>
template<std::ranges::contiguous_range R>
int Session<WEBSOCK>::write(const R& r) {
    return websock.send(r);
}

// -----------------------------------------------
// Select session type
// -----------------------------------------------

// For the template in the middle, see:
// http://stackoverflow.com/questions/1682844/templates-template-function-not-playing-well-with-classs-template-member-funct/1682885#1682885

template<typename... Tp>
std::tuple<int, Tp...> SessionAbstract::deserialize(Buffer<CMD_PAYLOAD_BUFFER_LEN>& payload) {
    switch (this->type) {
        case TCP:
            return static_cast<Session<TCP>*>(this)->template deserialize<Tp...>(payload);
        case UNIX:
            return static_cast<Session<UNIX>*>(this)->template deserialize<Tp...>(payload);
        case WEBSOCK:
            return static_cast<Session<WEBSOCK>*>(this)->template deserialize<Tp...>(payload);
        default:
            return std::tuple_cat(std::tuple{-1}, std::tuple<Tp...>());
    }
}

template<typename Tp>
int SessionAbstract::recv(Tp& container, Buffer<CMD_PAYLOAD_BUFFER_LEN>& payload) {
    switch (this->type) {
        case TCP:
            return static_cast<Session<TCP>*>(this)->recv(container, payload);
        case UNIX:
            return static_cast<Session<UNIX>*>(this)->recv(container, payload);
        case WEBSOCK:
            return static_cast<Session<WEBSOCK>*>(this)->recv(container, payload);
        default:
            return -1;
    }
}

template<uint16_t class_id, uint16_t func_id, typename... Args>
int SessionAbstract::send(Args&&... args) {
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
cast_to_session(const std::unique_ptr<SessionAbstract>& sess_abstract) {
    return static_cast<Session<socket_type>*>(sess_abstract.get());
}

} // namespace koheron

#endif // __KOHERON_SESSION_HPP__
