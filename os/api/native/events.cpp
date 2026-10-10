#include "events.hpp"
#include "server/network/sha1.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <sys/socket.h>

namespace koheron::management {
namespace {
std::string frame(unsigned opcode, std::string_view payload) {
    std::string result(1, static_cast<char>(0x80 | opcode));
    if (payload.size() < 126) result += static_cast<char>(payload.size());
    else if (payload.size() < 65536) {
        result += static_cast<char>(126); result += static_cast<char>(payload.size() >> 8); result += static_cast<char>(payload.size());
    } else {
        result += static_cast<char>(127);
        for (int shift = 56; shift >= 0; shift -= 8) result += static_cast<char>(static_cast<std::uint64_t>(payload.size()) >> shift);
    }
    result += payload; return result;
}
std::string base64(std::span<const unsigned char> bytes) {
    constexpr std::string_view alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string value;
    for (std::size_t i = 0; i < bytes.size(); i += 3) {
        const unsigned bits = (bytes[i] << 16) | (i + 1 < bytes.size() ? bytes[i + 1] << 8 : 0) | (i + 2 < bytes.size() ? bytes[i + 2] : 0);
        value += alphabet[(bits >> 18) & 63]; value += alphabet[(bits >> 12) & 63];
        value += i + 1 < bytes.size() ? alphabet[(bits >> 6) & 63] : '='; value += i + 2 < bytes.size() ? alphabet[bits & 63] : '=';
    }
    return value;
}
bool token(std::string_view header, std::string_view expected) {
    while (!header.empty()) {
        const auto end = header.find(','); auto value = trim(header.substr(0, end));
        std::ranges::transform(value, value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (value == expected) return true;
        if (end == header.npos) break;
        header.remove_prefix(end + 1);
    }
    return false;
}
MHD_Result reject(MHD_Connection* connection, unsigned status) {
    constexpr std::string_view message = "WebSocket handshake rejected\n";
    auto* response = MHD_create_response_from_buffer(message.size(), const_cast<char*>(message.data()), MHD_RESPMEM_PERSISTENT);
    if (!response) return MHD_NO;
    if (status == 426) MHD_add_response_header(response, "Sec-WebSocket-Version", "13");
    const auto result = MHD_queue_response(connection, status, response); MHD_destroy_response(response); return result;
}
}
EventHub::EventHub(std::function<std::string()> snapshot) : snapshot_(std::move(snapshot)), thread_([this] { run(); }) {}
EventHub::~EventHub() { stop(); }
void EventHub::wake() { std::lock_guard lock(mutex_); dirty_ = true; changed_.notify_one(); }
void EventHub::stop() {
    { std::lock_guard lock(mutex_); stopped_ = true; changed_.notify_one(); }
    if (thread_.joinable()) thread_.join();
}
void EventHub::accept(MHD_socket socket, MHD_UpgradeResponseHandle* handle, std::string_view input) {
    std::lock_guard lock(mutex_);
    if (stopped_ || clients_.size() >= 8 || input.size() > 512) { MHD_upgrade_action(handle, MHD_UPGRADE_ACTION_CLOSE); return; }
    clients_.push_back({socket, handle, std::string(input), {}, false, {}}); dirty_ = true; changed_.notify_one();
}
MHD_Result EventHub::upgrade(MHD_Connection* connection) {
    const auto header = [&](const char* name) { const char* value = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, name); return value ? std::string_view(value) : std::string_view(); };
    if (!token(header("Upgrade"), "websocket") || !token(header("Connection"), "upgrade")) return reject(connection, 426);
    if (header("Sec-WebSocket-Version") != "13") return reject(connection, 426);
    const auto key = header("Sec-WebSocket-Key");
    constexpr std::string_view alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    if (key.size() != 24 || !key.ends_with("==") || !std::ranges::all_of(key.substr(0, 22), [&](char c) { return alphabet.contains(c); }) ||
        alphabet.find(key[21]) % 16 != 0) return reject(connection, 400);
    // Browser connections must be same-origin; CLI clients may omit Origin.
    const auto origin = header("Origin"), host = header("Host");
    if (!origin.empty() && origin != "http://" + std::string(host) && origin != "https://" + std::string(host)) return reject(connection, 403);
    { std::lock_guard lock(mutex_); if (stopped_ || clients_.size() >= 8) return reject(connection, 503); }
    const std::string source = std::string(key) + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::array<unsigned char, 20> digest{};
    SHA1(reinterpret_cast<const unsigned char*>(source.data()), source.size(), digest.data());
    auto* response = MHD_create_response_for_upgrade([](void* context, MHD_Connection*, void*, const char* bytes, std::size_t count,
        MHD_socket socket, MHD_UpgradeResponseHandle* handle) { static_cast<EventHub*>(context)->accept(socket, handle, std::string_view(bytes, count)); }, this);
    if (!response) return MHD_NO;
    MHD_add_response_header(response, "Upgrade", "websocket"); MHD_add_response_header(response, "Connection", "Upgrade");
    MHD_add_response_header(response, "Sec-WebSocket-Accept", base64(digest).c_str());
    const auto result = MHD_queue_response(connection, 101, response); MHD_destroy_response(response); return result;
}
void EventHub::run() {
    auto next = std::chrono::steady_clock::now();
    for (;;) {
        std::unique_lock lock(mutex_);
        if (clients_.empty()) changed_.wait(lock, [&] { return stopped_ || !clients_.empty(); });
        changed_.wait_for(lock, std::chrono::milliseconds(50), [&] { return stopped_ || (dirty_ && !clients_.empty()); });
        if (stopped_) break;
        const bool publish = !clients_.empty() && (dirty_ || std::chrono::steady_clock::now() >= next);
        if (publish) {
            dirty_ = false; lock.unlock();
            std::string message;
            try { message = frame(1, snapshot_()); } catch (...) { /* HTTP fallback remains available. */ }
            lock.lock(); next = std::chrono::steady_clock::now() + std::chrono::seconds(2);
            for (auto& client : clients_) {
                if (!client.output.empty() || message.size() > 128 * 1024) client.closing = true;
                else client.output = message;
            }
        }
        for (auto& client : clients_) {
            const auto close = [&](unsigned code) {
                if (!client.closing) {
                    std::string payload; payload += static_cast<char>(code >> 8); payload += static_cast<char>(code);
                    client.output += frame(8, payload);
                    client.deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
                    client.closing = true;
                }
            };
            std::array<char, 512> bytes{};
            const auto count = ::recv(client.socket, bytes.data(), bytes.size(), MSG_DONTWAIT);
            if (count == 0 || (count < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) { client.closing = true; client.output.clear(); }
            if (count > 0 && !client.closing) client.input.append(bytes.data(), count);
            if (client.input.size() > 512) close(1009);
            while (!client.closing && client.input.size() >= 2) {
                const auto first = static_cast<unsigned char>(client.input[0]), second = static_cast<unsigned char>(client.input[1]);
                const unsigned opcode = first & 15, length = second & 127;
                if ((first & 0xf0) != 0x80 || !(second & 128) || length > 125 || (opcode == 8 && length == 1)) {
                    close(1002); break;
                }
                if (opcode != 8 && opcode != 9 && opcode != 10) { close(opcode == 1 || opcode == 2 ? 1003 : 1002); break; }
                if (client.input.size() < length + 6) break;
                std::string payload = client.input.substr(6, length);
                for (std::size_t i = 0; i < payload.size(); ++i) payload[i] ^= client.input[2 + i % 4];
                client.input.erase(0, length + 6);
                if (opcode == 8) {
                    if (payload.size() >= 2) {
                        const unsigned code = (static_cast<unsigned char>(payload[0]) << 8) | static_cast<unsigned char>(payload[1]);
                        if (!((code >= 1000 && code <= 1014 && code != 1004 && code != 1005 && code != 1006) || (code >= 3000 && code <= 4999))) { close(1002); break; }
                        if (utf8(payload.substr(2)) != payload.substr(2)) { close(1007); break; }
                    }
                    client.output += frame(8, payload); client.closing = true; client.deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
                }
                else if (opcode == 9) client.output += frame(10, payload);
            }
            if (!client.output.empty()) {
                const auto sent = ::send(client.socket, client.output.data(), client.output.size(), MSG_DONTWAIT | MSG_NOSIGNAL);
                if (sent > 0) client.output.erase(0, sent);
                else if (sent < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) client.closing = true;
            }
        }
        std::erase_if(clients_, [](const Client& client) {
            if (!client.closing || (!client.output.empty() && std::chrono::steady_clock::now() < client.deadline)) return false;
            MHD_upgrade_action(client.handle, MHD_UPGRADE_ACTION_CLOSE); return true;
        });
    }
    for (const auto& client : clients_) MHD_upgrade_action(client.handle, MHD_UPGRADE_ACTION_CLOSE);
    clients_.clear();
}
}
