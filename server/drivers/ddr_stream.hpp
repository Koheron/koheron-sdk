#ifndef __SERVER_DRIVERS_DDR_STREAM_HPP__
#define __SERVER_DRIVERS_DDR_STREAM_HPP__

#include "server/runtime/syslog.hpp"
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

#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <stdio.h>

class DdrStream
{
  public:
    DdrStream() {
        dfd = ::open("/dev/ddr_stream", O_RDONLY);

        if (dfd < 0) {
            logf<ERROR>("DdrStream: Cannot open /dev/ddr_stream\n");
        }
    }

    ~DdrStream() {
        stop();
    }

    bool start(uint16_t port, int backlog = SOMAXCONN) {
        bool expected = false;

        if (!running_.compare_exchange_strong(expected, true)) {
            log<WARNING>("DdrStream: already running\n");
            return false;
        }

        const int fd = net::create_tcp_listening_socket(port);

        if (fd < 0) {
            running_.store(false);
            return false;
        }

        if (::listen(fd, backlog) < 0) {
            logf<ERROR>("DdrStream: listen() failed (errno={})\n", errno);
            ::close(fd);
            running_.store(false);
            return false;
        }

        listen_fd_ = fd;
        accept_thread_ = std::thread(&DdrStream::accept_loop, this);
        logf("DdrStream: listening on port {}\n", port);
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

        log("DdrStream: server stopped\n");
    }

    bool publish() {
        if (!running_.load()) {
            log<WARNING>("DdrStream: publish() called while server stopped\n");
            return false;
        }

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
            if (send_all(fd)) {
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
                    logf<WARNING>("DdrStream: accept failed (errno={})\n", errno);
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }

                continue;
            }

            {
                std::scoped_lock lock(clients_mutex_);
                clients_.push_back(fd);
            }

            logf("DdrStream: client connected (fd={})\n", fd);
        }
    }

    bool send_all(int s) {
        constexpr size_t BLK = 1 << 20;
        // Rewind device read pointer so we start from offset 0 each publish
        if (::lseek(dfd, 0, SEEK_SET) < 0) {
            logf<ERROR>("DdrStream: lseek() failed errno={}\n", errno);
            return false;
        }

        int p[2];
        if (::pipe(p) < 0) {
            logf<ERROR>("pipe errno={}\n", errno);
            return false;
        }

        fcntl(p[0], F_SETPIPE_SZ, BLK); // 1 MiB
        fcntl(p[1], F_SETPIPE_SZ, BLK);

        int one = 1;
        setsockopt(s, IPPROTO_TCP, TCP_CORK, &one, sizeof(one));

        for (;;) {
            ssize_t n = ::splice(dfd, nullptr, p[1], nullptr, BLK, 0);
            if (n <= 0) {               // EOF (n==0) after 128 MiB → done
                if (n < 0) logf<ERROR>("splice(dev->pipe) errno={}\n", errno);
                break;
            }
            size_t remain = (size_t)n;
            while (remain > 0) {
                ssize_t m = ::splice(p[0], nullptr, s, nullptr, remain, 0);
                if (m <= 0) { /* handle EINTR/EAGAIN/EPIPE… */ break; }
                remain -= (size_t)m;
            }
        }

        one = 0;
        setsockopt(s, IPPROTO_TCP, TCP_CORK, &one, sizeof(one));

        ::close(p[0]);
        ::close(p[1]);
        return true;
    }

    std::atomic<bool> running_{false};
    int dfd;
    int listen_fd_{-1};
    std::thread accept_thread_;
    std::mutex clients_mutex_;
    std::vector<int> clients_;
};

#endif // __SERVER_DRIVERS_DDR_STREAM_HPP__