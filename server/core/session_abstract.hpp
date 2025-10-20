/// (c) Koheron

#ifndef __SESSION_ABSTRACT_HPP__
#define __SESSION_ABSTRACT_HPP__

#include "server/core/buffer.hpp"

#include <atomic>
#include <cstdint>
#include <tuple>

namespace koheron {

class SessionAbstract
{
  public:
    explicit SessionAbstract(int socket_type_)
    : type(socket_type_) {}

    virtual ~SessionAbstract() {}

    template<typename... Tp> std::tuple<int, Tp...> deserialize(Buffer<CMD_PAYLOAD_BUFFER_LEN>& payload);
    template<typename Tp> int recv(Tp& container, Buffer<CMD_PAYLOAD_BUFFER_LEN>& payload);
    template<uint16_t class_id, uint16_t func_id, typename... Args> int send(Args&&... args);

    int type;

    std::atomic<bool> exit_signal{false};

    void exit_comm() {
        exit_signal = true;
    }

};

} // namespace koheron

#endif // __SESSION_ABSTRACT_HPP__