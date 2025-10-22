/// Commands received by the server
///
/// (c) Koheron

#ifndef __COMMANDS_HPP__
#define __COMMANDS_HPP__

#include "server/runtime/syslog.hpp"
#include "server/runtime/drivers_table.hpp"
#include "server/network/configs/server_definitions.hpp"
#include "server/network/buffer.hpp"
#include "server/network/session.hpp"
#include "server/utilities/concepts.hpp"
#include "server/utilities/meta_utils.hpp"

#include <cstdint>
#include <utility>
#include <tuple>
#include <initializer_list>

namespace net {

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

// ---- detect dynamic-size args (vector/string) ----
template<class T> struct is_dynamic_arg : std::false_type {};
template<class U, class A> struct is_dynamic_arg<std::vector<U,A>> : std::true_type {};
template<class Ch, class Tr, class A>
struct is_dynamic_arg<std::basic_string<Ch,Tr,A>> : std::true_type {};

template<class Tuple, std::size_t... I>
consteval bool all_args_static(std::index_sequence<I...>) {
    return (... && !is_dynamic_arg<std::decay_t<std::tuple_element_t<I, Tuple>>>::value);
}

// ---- buffer size computation for fixed-size args ----
template<class Tuple, std::size_t... I>
constexpr std::size_t required_size_over_tuple(std::index_sequence<I...>) {
    return required_buffer_size<std::decay_t<std::tuple_element_t<I, Tuple>>...>();
}

// ---- “make decayed tuple from index sequence” meta ----
template<class Tuple, class Seq> struct decayed_tuple_from_seq;
template<class Tuple, std::size_t... I>
struct decayed_tuple_from_seq<Tuple, std::index_sequence<I...>> {
    using type = std::tuple<std::decay_t<std::tuple_element_t<I, Tuple>>...>;
};

class Command
{
  public:
    Command() noexcept
    : header(HEADER_START)
    {}

    // op_invoke:
    // Invoke a pointer-to-member-function (pmf) of class C.
    // If the function does not return void it sends the result back to the caller.

    // Non-const member functions
    template<class C, class Ret, class... Args>
    int op_invoke(C& obj, Ret (C::*pmf)(Args...)) {
        return op_invoke_impl(obj, pmf);
    }

    // const member functions
    template<class C, class Ret, class... Args>
    int op_invoke(const C& obj, Ret (C::*pmf)(Args...) const) {
        return op_invoke_impl(obj, pmf);
    }

    template<typename... Args>
    int send(Args&&... args) {
        return session->send(driver, operation, std::forward<Args>(args)...);
    }

    template <typename T>
    bool read_one(T& v) {
        if constexpr (ut::resizableContiguousRange<T>) {
            return recv(v) >= 0;
        } else { // fixed-size / POD-ish types
            auto [status, value] = deserialize<T>();

            if (status < 0) {
                return false;
            }

            v = value;
            return true;
        }
    }

    rt::driver_id driver = 0;    // The driver to control
    uint16_t operation = 0; // Operation ID
    SessionID session_id = -1;   // ID of the session emitting the command

  private:
    Session *session = nullptr;  // Pointer to the session emitting the command
    int socket_type = -1;
    int comm_fd = -1;

    enum Header : uint32_t {
        HEADER_SIZE = 8,
        HEADER_START = 4  // First 4 bytes are reserved
    };

    Buffer<HEADER_SIZE> header; // Raw data header
    Buffer<CMD_PAYLOAD_BUFFER_LEN> payload;

    template<typename... Tp>
    std::tuple<int, Tp...> deserialize() {
        if constexpr (sizeof...(Tp) == 0) {
            return std::tuple{0};
        }

        constexpr auto pack_len = required_buffer_size<Tp...>();

        if (socket_type == TCP || socket_type == UNIX) {
            Buffer<pack_len> buff;
            const auto err = read_exact(
                comm_fd,
                std::as_writable_bytes(std::span{buff.data(), pack_len})
            );
            session->rx_tracker.update(err);
            return std::tuple_cat(std::tuple{err}, buff.template deserialize<Tp...>());
        } else if (socket_type == WEBSOCK) {
            return std::tuple_cat(std::tuple{0}, payload.deserialize<Tp...>());
            session->rx_tracker.update(pack_len);
        } else {
            return std::tuple_cat(std::tuple{-1}, std::tuple<Tp...>());
        }
    }

    int64_t get_pack_length() {
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

    template<ut::resizableContiguousRange R>
    int recv(R& c) {
        if (socket_type == TCP || socket_type == UNIX) {
            // Read data directly from socket
            using T = R::value_type;
            const auto length = get_pack_length() / sizeof(T);

            if (length < 0) {
                return -1;
            }

            c.resize(length);
            const auto nbytes = read_exact(
                comm_fd,
                std::as_writable_bytes(
                    std::span{c.data(), static_cast<std::size_t>(length)})
            );

            if (nbytes >= 0) {
                session->rx_tracker.update(nbytes + sizeof(uint32_t));
                logf<DEBUG>("TCPSocket: Received a container of {} bytes\n", length);
            }

            return nbytes;
        } else if (socket_type == WEBSOCK) {
            // Data already stored in payload
            const auto [length] = payload.deserialize<uint32_t>();

            if (length > CMD_PAYLOAD_BUFFER_LEN) {
                log<ERROR>("WebSocket::rcv dynamic container: Payload size overflow\n");
                return -1;
            }

            payload.to_container(c, length);
            session->rx_tracker.update(length);
            return 0;
        } else {
            return -1;
        }
    }

    template <class Tuple, std::size_t... I>
    bool read_arguments(Tuple& args, std::index_sequence<I...>) {
        bool ok = true;
        (void)std::initializer_list<int>{ (ok = ok && read_one(std::get<I>(args)), 0)... };
        return ok;
    }

    template<class Obj, class PMF>
    int op_invoke_impl(Obj&& obj, PMF pmf) {
        using traits     = pmf_traits<PMF>;
        using Ret        = typename traits::ret;
        using ArgsTuple  = typename traits::args;

        constexpr std::size_t N = std::tuple_size_v<ArgsTuple>;

        // Only assert static buffer size if all args are fixed-size
        if constexpr (N > 0 && all_args_static<ArgsTuple>(std::make_index_sequence<N>{})) {
            constexpr auto need = required_size_over_tuple<ArgsTuple>(std::make_index_sequence<N>{});
            static_assert(need <= CMD_PAYLOAD_BUFFER_LEN, "Buffer size too small");
        }

        if constexpr (N == 0) {
            // No payload to read
            if constexpr (std::is_void_v<Ret>) {
                std::invoke(pmf, std::forward<Obj>(obj));
                return 0;
            } else {
                decltype(auto) r = std::invoke(pmf, std::forward<Obj>(obj));
                return send(std::forward<decltype(r)>(r));
            }
        } else {
            using IS = std::make_index_sequence<N>;
            using DecayedArgsTuple = typename decayed_tuple_from_seq<ArgsTuple, IS>::type;

            DecayedArgsTuple args{};
            if (!read_arguments(args, IS{})) {
                return -1;
            }

            if constexpr (std::is_void_v<Ret>) {
                std::apply([&](auto&... a) {
                    std::invoke(pmf, std::forward<Obj>(obj), a...);
                }, args);
                return 0;
            } else {
                decltype(auto) r = std::apply([&](auto&... a) -> decltype(auto) {
                    return std::invoke(pmf, std::forward<Obj>(obj), a...);
                }, args);
                return send(std::forward<decltype(r)>(r));
            }
        }
    }

    friend class Session;
    template<int socket_type> friend class SocketSession;
};

} // namespace net

#endif // __COMMANDS_HPP__
