#include "common.hpp"

#include <arpa/inet.h>
#include <array>
#include <charconv>
#include <csignal>
#include <iostream>
#include <limits>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <thread>
#include <unistd.h>

using namespace koheron::management;
namespace {
bool has_ipv4() {
    Fd probe(::socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0));
    if (probe.get() < 0) system_error("IPv4 socket");
    for (const auto* name : {"end0", "eth0"}) {
        ifreq interface{};
        std::char_traits<char>::copy(interface.ifr_name, name, std::char_traits<char>::length(name));
        if (::ioctl(probe.get(), SIOCGIFADDR, &interface) == 0 &&
            reinterpret_cast<const sockaddr_in*>(&interface.ifr_addr)->sin_addr.s_addr != INADDR_ANY) return true;
    }
    return false;
}
void receive(int fd, std::span<char> bytes) {
    while (!bytes.empty()) {
        const auto count = ::recv(fd, bytes.data(), bytes.size(), 0);
        if (count < 0) { if (errno == EINTR) continue; system_error("LED receive"); }
        if (count == 0) throw std::runtime_error("Instrument connection closed");
        bytes = bytes.subspan(static_cast<std::size_t>(count));
    }
}
void send(int fd, std::span<const char> bytes) {
    while (!bytes.empty()) {
        const auto count = ::send(fd, bytes.data(), bytes.size(), MSG_NOSIGNAL);
        if (count < 0) { if (errno == EINTR) continue; system_error("LED send"); }
        if (count == 0) throw std::runtime_error("Instrument connection closed");
        bytes = bytes.subspan(static_cast<std::size_t>(count));
    }
}
void command(int fd, std::uint16_t driver, std::uint16_t operation) {
    std::array<char, 8> bytes{};
    bytes[4] = static_cast<char>(driver >> 8); bytes[5] = static_cast<char>(driver);
    bytes[6] = static_cast<char>(operation >> 8); bytes[7] = static_cast<char>(operation);
    send(fd, bytes);
}
unsigned identifier(json_object* object) {
    json_object* value = nullptr;
    if (!json_object_object_get_ex(object, "id", &value) || !json_object_is_type(value, json_type_int))
        throw std::runtime_error("Missing driver/command ID");
    const auto id = json_object_get_int64(value);
    if (id < 0 || id > std::numeric_limits<std::uint16_t>::max()) throw std::runtime_error("Invalid driver/command ID");
    return static_cast<unsigned>(id);
}
}
int main(int argc, char** argv) {
    try {
        const std::string path = argc > 1 ? argv[1] : "/run/koheron-server.sock";
        if (argc > 3 || (argc == 3 && std::string_view(argv[2]) != "--skip-ip-wait"))
            throw std::runtime_error("usage: koheron-server-init [SOCKET [--skip-ip-wait]]");
        sockaddr_un address{};
        address.sun_family = AF_UNIX;
        if (path.size() >= sizeof(address.sun_path)) throw std::runtime_error("Socket path too long");
        std::char_traits<char>::copy(address.sun_path, path.c_str(), path.size() + 1);
        Fd socket(::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0));
        if (socket.get() < 0 || ::connect(socket.get(), reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) system_error("Instrument socket");
        command(socket.get(), 1, 1);
        std::array<char, 12> header{};
        receive(socket.get(), header);
        std::uint32_t length = 0;
        for (std::size_t i = 8; i < 12; ++i) length = (length << 8) | static_cast<unsigned char>(header[i]);
        if (length > 4 * 1024 * 1024) throw std::runtime_error("Driver metadata exceeds size limit");
        std::string payload(length, '\0');
        receive(socket.get(), std::span(payload.data(), payload.size()));
        auto drivers = parse(payload);
        if (!json_object_is_type(drivers.get(), json_type_array)) throw std::runtime_error("Invalid driver metadata");
        for (std::size_t i = 0; i < json_object_array_length(drivers.get()); ++i) {
            auto* driver = json_object_array_get_idx(drivers.get(), i);
            if (field(driver, "class") != "Common") continue;
            json_object* functions = nullptr;
            if (!json_object_object_get_ex(driver, "functions", &functions) || !json_object_is_type(functions, json_type_array)) return 0;
            for (std::size_t j = 0; j < json_object_array_length(functions); ++j) {
                auto* operation = json_object_array_get_idx(functions, j);
                if (field(operation, "name") != "ip_on_leds") continue;
                if (argc < 3 && !has_ipv4()) {
                    std::cout << "Waiting for an IPv4 address for the IP LEDs" << std::endl;
                    while (!has_ipv4()) std::this_thread::sleep_for(std::chrono::seconds(1));
                }
                command(socket.get(), static_cast<std::uint16_t>(identifier(driver)), static_cast<std::uint16_t>(identifier(operation)));
                return 0;
            }
        }
        return 0;
    } catch (const std::exception& error) { std::cerr << "LED initialization failed: " << error.what() << '\n'; return 1; }
}
