#include "server/client/koheron-client.hpp"
#include <cerrno>
#include <dirent.h>
#include <fcntl.h>
#include <iostream>

static_assert(!std::is_copy_constructible_v<KoheronClient>);
static_assert(!std::is_copy_assignable_v<KoheronClient>);
static_assert(std::is_nothrow_move_constructible_v<KoheronClient>);
static_assert(std::is_nothrow_move_assignable_v<KoheronClient>);
static_assert(noexcept(std::declval<KoheronClient&>().close()));

void check(bool condition, const char* message) {
    if (!condition) { std::cerr << message << '\n'; std::exit(1); }
}

size_t descriptor_count() {
    DIR* directory = opendir("/proc/self/fd");
    check(directory != nullptr, "cannot inspect descriptors");
    size_t count = 0;
    while (auto* entry = readdir(directory)) {
        if (entry->d_name[0] != '.') ++count;
    }
    closedir(directory);
    return count;
}

void exchange(KoheronClient& client) {
    client.call_rt(2, 0, uint32_t{42});
    client.call_by_name("TestDriver::get_value");
    check(client.recv_by_name<uint32_t>("TestDriver::get_value") == 42,
          "connection or metadata lost during ownership transfer");
}

int main(int argc, char** argv) {
    check(argc == 3, "port and case required");
    const int port = std::stoi(argv[1]);
    const std::string_view mode = argv[2];
    const auto initial = descriptor_count();
    if (mode == "close" || mode == "zero") {
        if (mode == "zero") ::close(STDIN_FILENO);
        int guard;
        {
            KoheronClient client("127.0.0.1", port);
            client.connect();
            exchange(client);
            client.close();
            client.close();
            if (mode == "zero") {
                check(fcntl(STDIN_FILENO, F_GETFD) < 0 && errno == EBADF,
                      "descriptor zero was not released");
            }
            guard = ::open("/dev/null", O_RDONLY);
            check(guard >= 0, "cannot reserve released descriptor");
        }
        check(fcntl(guard, F_GETFD) >= 0, "destructor closed a reused descriptor");
        ::close(guard);
        return 0;
    }
    if (mode == "move" || mode == "assign") {
        auto source = std::make_unique<KoheronClient>("127.0.0.1", port);
        source->connect();
        if (mode == "move") {
            KoheronClient moved(std::move(*source));
            source.reset();
            exchange(moved);
        } else {
            KoheronClient target("127.0.0.1", port);
            target.connect();
            target = std::move(*source);
            source.reset();
            exchange(target);
        }
    } else if (mode == "reconnect") {
        KoheronClient client("127.0.0.1", port);
        for (int i = 0; i < 3; ++i) {
            client.connect();
            exchange(client);
            check(descriptor_count() == initial + 1, "reconnect leaked a socket");
        }
    } else if (mode == "refused") {
        // Reserve a local port without listening so connection failure is deterministic.
        const int reserved = ::socket(AF_INET, SOCK_STREAM, 0);
        check(reserved >= 0, "cannot reserve local port");
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        check(bind(reserved, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0,
              "cannot bind local port");
        socklen_t length = sizeof(address);
        check(getsockname(reserved, reinterpret_cast<sockaddr*>(&address), &length) == 0,
              "cannot inspect local port");
        {
            KoheronClient client("127.0.0.1", ntohs(address.sin_port));
            for (int i = 0; i < 2; ++i) {
                bool rejected = false;
                try { client.connect(); }
                catch (const socket_error& error) {
                    rejected = true;
                    check(std::string_view(error.what()) == "Cannot connect to server\n",
                          "cleanup replaced the connection error");
                }
                check(rejected, "connection to non-listening port succeeded");
                check(descriptor_count() == initial + 1, "connection failure leaked a socket");
            }
        }
        ::close(reserved);
    } else if (mode == "context") {
        KoheronClient client("127.0.0.1", port);
        bool rejected = false;
        try { client.connect(); }
        catch (const socket_error&) { rejected = true; }
        check(rejected, "missing metadata accepted");
        check(descriptor_count() == initial, "metadata failure leaked a socket");
    } else check(false, "unknown case");
    check(descriptor_count() == initial, "destruction leaked a socket");
}
