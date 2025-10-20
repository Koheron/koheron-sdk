/// (c) Koheron

#ifndef __SESSION_ABSTRACT_HPP__
#define __SESSION_ABSTRACT_HPP__

#include "server/core/buffer.hpp"
#include "server/core/serializer_deserializer.hpp"

#include <atomic>
#include <cstdint>
#include <tuple>

namespace koheron {

class SessionAbstract
{
  public:
    explicit SessionAbstract(int socket_type_)
    : type(socket_type_) {}

    virtual ~SessionAbstract() = default;

    template<uint16_t class_id, uint16_t func_id, typename... Args>
    int send(Args&&... args) {
        // build into base-owned buffer
        builder.reset_into(send_buffer);
        builder.template write_header<class_id, func_id>();
        builder.push(std::forward<Args>(args)...);

        int n = write_bytes(std::as_bytes(std::span{send_buffer}));

        if (n == 0) {
            status = CLOSED;
        }

        return n;
    }

    virtual void shutdown() = 0;
    int type;

    std::atomic<bool> exit_signal{false};

    void exit_comm() {
        exit_signal = true;
    }

  protected:
    enum {CLOSED, OPENED};
    int status = OPENED;
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

} // namespace koheron

#endif // __SESSION_ABSTRACT_HPP__