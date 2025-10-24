/// Network mapped memory driver
///
/// (c) Koheron

#ifndef __SERVER_DRIVERS_NETWORK_MAPPED_MEMORY_HPP__
#define __SERVER_DRIVERS_NETWORK_MAPPED_MEMORY_HPP__

#include "server/runtime/syslog.hpp"
#include "server/hardware/memory_manager.hpp"
#include "server/network/sockets.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>
#include <thread>
#include <vector>

#include <errno.h>
#include <sys/socket.h>
#include <unistd.h>

/// TCP server that streams the contents of a catalogued memory region.
///
/// The driver holds a reference to the memory mapped by the `hw::MemoryManager`
/// service and exposes a small API to start/stop a TCP server that lets clients
/// subscribe to updates of that region.  Once started, the driver accepts
/// connections and keeps track of the active clients so that `publish()` can
/// forward the selected span of the memory to every subscriber.
///
/// The class is header-only so it can be instantiated directly from user code
/// without touching the build system.  Example usage:
///
/// ```
/// net::drivers::NetworkMappedMemory<mem::adc> publisher;
/// publisher.start(9000);
/// // ... whenever DMA fills the buffer:
/// publisher.publish();
/// ```
///
/// The driver focuses on wiring the memory map to the transport layer; higher
/// level concerns such as metadata discovery are intentionally left to the
/// embedding application.
template<MemID id>
class NetworkMappedMemory
{
  public:
    NetworkMappedMemory()
    : memory_(hw::get_memory<id>())
    {}

    ~NetworkMappedMemory() {
        stop();
    }

    /// Start the TCP server on the provided port.
    ///
    /// @param port Listening port (host byte order).
    /// @param backlog Listen backlog.  Defaults to `SOMAXCONN`.
    /// @return `true` on success, `false` otherwise.
    bool start(uint16_t port, int backlog = SOMAXCONN) {
        bool expected = false;

        if (!running_.compare_exchange_strong(expected, true)) {
            logf<WARNING>("NetworkMappedMemory[{}]: already running\n", hw::Memory<id>::name);
            return false;
        }

        const int fd = net::create_tcp_listening_socket(port);

        if (fd < 0) {
            running_.store(false);
            return false;
        }

        if (::listen(fd, backlog) < 0) {
            logf<ERROR>("NetworkMappedMemory[{}]: listen() failed (errno={})\n", hw::Memory<id>::name, errno);
            ::close(fd);
            running_.store(false);
            return false;
        }

        listen_fd_ = fd;
        accept_thread_ = std::thread(&NetworkMappedMemory::accept_loop, this);
        logf("NetworkMappedMemory[{}]: listening on port {}\n", hw::Memory<id>::name, port);
        return true;
    }

    /// Stop the TCP server and close all active connections.
    void stop() {
        if (!running_.exchange(false)) {
            return;
        }

        if (listen_fd_ >= 0) {
            ::shutdown(listen_fd_, SHUT_RDWR);
            ::close(listen_fd_);
            listen_fd_ = -1;
        }

        if (accept_thread_.joinable()) {
            accept_thread_.join();
        }

        std::vector<int> clients;

        {
            std::scoped_lock lock(clients_mutex_);
            clients.swap(clients_);
        }

        for (int fd : clients) {
            ::shutdown(fd, SHUT_RDWR);
            ::close(fd);
        }

        logf<INFO>("NetworkMappedMemory[{}]: server stopped\n", hw::Memory<id>::name);
    }

    /// Publish a slice of the mapped memory to every connected client.
    ///
    /// @param offset Start offset within the mapped region.
    /// @param length Number of bytes to send.  Defaults to the remaining bytes
    ///               from `offset` to the end of the region.
    /// @return `true` if at least one client received the entire payload.
    bool publish(std::size_t offset = 0, std::size_t length = std::numeric_limits<std::size_t>::max()) {
        if (!running_.load()) {
            logf<WARNING>("NetworkMappedMemory[{}]: publish() called while server stopped\n", hw::Memory<id>::name);
            return false;
        }

        const std::size_t mapped_size = memory_.mapped_size();

        if (offset >= mapped_size) {
            logf<ERROR>("NetworkMappedMemory[{}]: invalid offset {} (size={})\n", hw::Memory<id>::name, offset, mapped_size);
            return false;
        }

        if (length == std::numeric_limits<std::size_t>::max() || offset + length > mapped_size) {
            length = mapped_size - offset;
        }

        const auto* base = reinterpret_cast<const std::byte*>(memory_.base_addr());
        const auto* payload = base + offset;

        std::vector<int> clients_snapshot;

        {
            std::scoped_lock lock(clients_mutex_);
            clients_snapshot = clients_;
        }

        if (clients_snapshot.empty()) {
            return false;
        }

        bool delivered = false;
        std::vector<int> failed;

        for (int fd : clients_snapshot) {
            if (send_all(fd, payload, length)) {
                delivered = true;
            } else {
                failed.push_back(fd);
            }
        }

        if (!failed.empty()) {
            std::scoped_lock lock(clients_mutex_);

            for (int bad_fd : failed) {
                auto it = std::find(clients_.begin(), clients_.end(), bad_fd);

                if (it != clients_.end()) {
                    ::shutdown(*it, SHUT_RDWR);
                    ::close(*it);
                    clients_.erase(it);
                }
            }
        }

        return delivered;
    }

  private:
    void accept_loop() {
        while (running_.load()) {
            const int fd = net::open_tcp_communication(listen_fd_);

            if (fd < 0) {
                if (running_.load()) {
                    logf<WARNING>("NetworkMappedMemory[{}]: accept failed (errno={})\n", hw::Memory<id>::name, errno);
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }

                continue;
            }

            {
                std::scoped_lock lock(clients_mutex_);
                clients_.push_back(fd);
            }

            logf("NetworkMappedMemory[{}]: client connected (fd={})\n", hw::Memory<id>::name, fd);
        }
    }

    static bool send_all(int fd, const std::byte* data, std::size_t length) {
        const auto* ptr = data;
        std::size_t remaining = length;

        while (remaining > 0) {
            const ssize_t sent = ::send(fd, ptr, remaining, MSG_NOSIGNAL);

            if (sent < 0) {
                if (errno == EINTR) {
                    continue;
                }

                return false;
            }

            ptr += static_cast<std::size_t>(sent);
            remaining -= static_cast<std::size_t>(sent);
        }

        return true;
    }

    hw::Memory<id>& memory_;
    std::atomic<bool> running_{false};
    int listen_fd_{-1};
    std::thread accept_thread_;
    std::mutex clients_mutex_;
    std::vector<int> clients_;
};

#endif // __SERVER_DRIVERS_NETWORK_MAPPED_MEMORY_HPP__
