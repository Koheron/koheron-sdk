/// (c) Koheron

#ifndef __KOHERON_SESSION_HPP__
#define __KOHERON_SESSION_HPP__

#include "server/network/configs/server_definitions.hpp"
#include "server/network/serializer_deserializer.hpp"
#include "server/utilities/rate_tracker.hpp"
#include "server/utilities/metadata.hpp"
#include "server/utilities/meta_utils.hpp"

#include <array>
#include <atomic>
#include <cstdint>
#include <cstddef>
#include <tuple>
#include <span>
#include <memory_resource>
#include <sys/socket.h>

namespace net {

class Command;

class Session
{
  public:
    explicit Session(int socket_type_)
    : type(socket_type_) {
        infos.set("socket_flag", socket_type_);
        infos.set("socket_type", listen_channel_desc[socket_type_]);
    }

    virtual ~Session() = default;

    template<typename... Args>
    int send(uint16_t class_id, uint16_t func_id, Args&&... args) {
        constexpr auto nargs = sizeof...(Args);
        constexpr auto sock_flags = MSG_NOSIGNAL | MSG_ZEROCOPY;
        // constexpr auto sock_flags = MSG_NOSIGNAL;

        if constexpr (nargs == 0) {
            return 0;
        }

        // build into base-owned buffer
        builder.reset_into(send_buffer);
        builder.write_header(class_id, func_id);

        using first_t = std::tuple_element_t<0, std::tuple<Args...>>;

        if constexpr (is_std_span_v<first_t>) {
            auto&& span = std::get<0>(std::forward_as_tuple(args...));

            if constexpr (first_t::extent == std::dynamic_extent) {
                // Dynamic size => append size to header
                builder.push(span.size_bytes());
            }

            return send_payload(span, sock_flags);
        } else if constexpr (is_std_array_v<first_t> && nargs == 1) {
            auto&& array = std::get<0>(std::forward_as_tuple(args...));
            return send_payload(std::span{array}, sock_flags);
        } else if constexpr (is_std_vector_v<first_t> && nargs == 1) {
            using vec_t = std::remove_cvref_t<first_t>;
            using value_t = vec_t::value_type;

            auto&& vect = std::get<0>(std::forward_as_tuple(args...));
            builder.push(vect.size() * sizeof(value_t));
            return send_payload(std::span{vect}, sock_flags);
        } else {
            // Small/heterogeneous payload: serialize all and single send payload, serialize everything + single send
            builder.push(std::forward<Args>(args)...);

            int n = write_bytes(std::as_bytes(std::span{send_buffer}));
            tx_tracker.update(n);

            if (n == 0) {
                status = CLOSED;
            }

            return n;
        }
    }

    int run();

    virtual void shutdown() = 0;
    void exit_comm() { exit_signal = true; }

    int type;
    std::atomic<bool> exit_signal{false};

    // Infos
    // Metadata are publicly available,
    // users can expand it to fit extra requirements (for example authentification)
    ut::Metadata<> infos;

    void log_infos() {
        log("Session infos:\n");
        infos.log("    ");
    }

    // Data rates
    auto rates() {
        return std::array{
            rx_tracker.snapshot(),
            tx_tracker.snapshot()
        };
    }

    void log_rates() {
        rx_tracker.log_snapshot("RX Rates");
        tx_tracker.log_snapshot("TX Rates");
    }

  protected:
    enum {CLOSED, OPENED};
    int status = OPENED;

    // Socket specific hooks
    virtual int init_socket() = 0;
    virtual int exit_socket() = 0;
    virtual int read_command(Command& cmd) = 0;
    virtual int write_bytes(std::span<const std::byte>) = 0;
    virtual int send_iov(std::span<const std::byte> header,
                std::span<const std::byte> payload,
                int flags) = 0;

    // First 4 KiB of allocations come from initial_storage, no heap at all
    // If we exceed it will grab memory from the system allocator.
    std::array<std::byte, 4096> seed{};
    std::pmr::monotonic_buffer_resource pool{
        seed.data(), seed.size(), std::pmr::get_default_resource()
    };
    std::pmr::vector<unsigned char> send_buffer{ &pool };
    CommandBuilder builder;

    ut::RateTracker rx_tracker{5, 64, 1.5}; // 5s window, keep 64s history, 1.5s EWMA half-life
    ut::RateTracker tx_tracker{5, 64, 1.5};

    friend class Command;

    template<class T, std::size_t Extent>
    int send_payload(std::span<T, Extent> payload, int flags) {
        auto header = std::as_bytes(std::span{send_buffer});
        const auto t0 = ut::RateTracker::clock::now();
        int n = send_iov(header, std::as_bytes(payload), flags);
        auto dur = ut::RateTracker::clock::now() - t0;

        if (n <= 0) {
            if (n == 0) {
                status = CLOSED;
            }

            return n;
        }

        tx_tracker.update_over_duration(n, dur);
        return n;
    }
};

} // namespace net

#endif // __KOHERON_SESSION_HPP__
