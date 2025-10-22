/// (c) Koheron

#ifndef __KOHERON_SESSION_HPP__
#define __KOHERON_SESSION_HPP__

#include "server/network/configs/server_definitions.hpp"
#include "server/network/serializer_deserializer.hpp"
#include "server/utilities/metadata.hpp"

#include <array>
#include <atomic>
#include <cstdint>
#include <cstddef>
#include <tuple>
#include <span>
#include <memory_resource>

namespace net {

class Command;

class Session
{
  public:
    explicit Session(int socket_type_)
    : type(socket_type_) {
        metadata.set("socket_type", listen_channel_desc[socket_type_]);
    }

    virtual ~Session() = default;

    template<typename... Args>
    int send(uint16_t class_id, uint16_t func_id, Args&&... args) {
        // build into base-owned buffer
        builder.reset_into(send_buffer);
        builder.write_header(class_id, func_id);
        builder.push(std::forward<Args>(args)...);

        int n = write_bytes(std::as_bytes(std::span{send_buffer}));

        if (n == 0) {
            status = CLOSED;
        }

        return n;
    }

    int run();

    virtual void shutdown() = 0;
    void exit_comm() { exit_signal = true; }

    int type;
    std::atomic<bool> exit_signal{false};
    ut::Metadata<> metadata;
  protected:
    enum {CLOSED, OPENED};
    int status = OPENED;

    // Socket specific hooks
    virtual int init_socket() = 0;
    virtual int exit_socket() = 0;
    virtual int read_command(Command& cmd) = 0;
    virtual int write_bytes(std::span<const std::byte>) = 0;

    // First 4 KiB of allocations come from initial_storage, no heap at all
    // If we exceed it will grab memory from the system allocator.
    std::array<std::byte, 4096> seed{};
    std::pmr::monotonic_buffer_resource pool{
        seed.data(), seed.size(), std::pmr::get_default_resource()
    };
    std::pmr::vector<unsigned char> send_buffer{ &pool };
    CommandBuilder builder;
};

} // namespace net

#endif // __KOHERON_SESSION_HPP__
