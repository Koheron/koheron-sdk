/// Implementation of websocket.hpp
///
/// Inspired by https://github.com/MarkusPfundstein/C---Websocket-Server
///
/// (c) Koheron

#include "server/core/websocket.hpp"
#include "server/core/commands.hpp"
#include "server/utilities/base64.hpp"
#include "server/utilities/sha1.hpp"
#include "server/runtime/syslog.hpp"
#include "server/utilities/endian_utils.hpp"

#include <array>
#include <string>
#include <string_view>
#include <span>
#include <cstdint>
#include <cstring>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <unistd.h>

using namespace std::string_view_literals;

namespace koheron {

WebSocket::WebSocket()
  : comm_fd(-1)
  , read_str_len(0)
  , connection_closed(false)
{
    std::memset(sha_str, 0, 21);
}

void WebSocket::set_id(int comm_fd_) {
    comm_fd = comm_fd_;
}

int WebSocket::authenticate() {
    if (read_http_packet() < 0) {
        return -1;
    }

    constexpr auto WSKeyId  = "Sec-WebSocket-Key: "sv;
    constexpr auto WSProtId = "Sec-WebSocket-Protocol: "sv;
    constexpr auto WSMagic  = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"sv;
    constexpr size_t WSKeyLen = 24; // per RFC: base64 of 16 bytes => 24 chars
    constexpr size_t WSMagicLen = WSMagic.size();

    const bool is_protocol = (http_packet.find(WSProtId) != std::string_view::npos);
    const size_t pos = http_packet.find(WSKeyId);

    if (pos == std::string_view::npos) {
        log<CRITICAL>("WebSocket: No Sec-WebSocket-Key header");
        return -1;
    }

    const size_t key_begin = pos + WSKeyId.size();

    if (key_begin + WSKeyLen > http_packet.size()) {
        log<CRITICAL>("WebSocket: Truncated Sec-WebSocket-Key");
        return -1;
    }

    const std::string_view key_view = http_packet.substr(key_begin, WSKeyLen);

    // Concatenate key + magic into a small stack buffer (24 + 36 = 60 bytes)
    std::array<char, WSKeyLen + WSMagicLen> concat;
    std::memcpy(concat.data(), key_view.data(), WSKeyLen);
    std::memcpy(concat.data() + WSKeyLen, WSMagic.data(), WSMagicLen);

    // SHA1(concat) => base64
    unsigned char sha[20];
    SHA1(reinterpret_cast<const unsigned char*>(concat.data()), concat.size(), sha);
    std::string accept_b64 = base64_encode(sha, sizeof(sha));

    // Build response
    std::string resp;
    resp.reserve(160);
    resp += "HTTP/1.1 101 Switching Protocols\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Accept: ";
    resp += accept_b64;
    resp += "\r\n";

    if (is_protocol) {
        resp += "Sec-WebSocket-Protocol: chat\r\n";
    }

    resp += "\r\n";

    return send_request(resp);
}

int WebSocket::read_http_packet() {
    reset_read_buff();

    const ssize_t nb_bytes_rcvd = ::read(comm_fd, read_str.data(), read_str.size());

    // Check reception ...
    if (nb_bytes_rcvd < 0) {
        log<CRITICAL>("WebSocket: Read error\n");
        return -1;
    }

    if (nb_bytes_rcvd == KOHERON_READ_STR_LEN) {
        log<CRITICAL>("WebSocket: Read buffer overflow\n");
        return -1;
    }

    if (nb_bytes_rcvd == 0) { // Connection closed by client
        connection_closed = true;
        return -1;
    }

    http_packet = std::string_view(read_str.data(), static_cast<size_t>(nb_bytes_rcvd));

    if (http_packet.find("\r\n\r\n") == std::string_view::npos) {
        if (static_cast<size_t>(nb_bytes_rcvd) == read_str.size()) {
            log<CRITICAL>("WebSocket: HTTP header too large for buffer\n");
        } else {
            log<CRITICAL>("WebSocket: Incomplete HTTP header\n");
        }
        return -1;
    }

    log<DEBUG>("[R] HTTP header\n");
    return static_cast<int>(nb_bytes_rcvd);
}

int WebSocket::set_send_header(int64_t data_len, unsigned int format) {
    if (data_len < 0) [[unlikely]] {
        log<CRITICAL>("WebSocket: negative payload length\n");
        return -1;
    }

    std::span<uint8_t> b(send_buf.data(), send_buf.size());
    b[0] = static_cast<uint8_t>(format);

    if (data_len <= SMALL_STREAM) {
        b[1] = static_cast<uint8_t>(data_len);
        return SMALL_OFFSET;
    }

    if (data_len <= 0xFFFF) {
        b[1] = MEDIUM_STREAM;
        // Write 16-bit big-endian length into b[2], b[3]
        to_big_endian_bytes<uint64_t>(data_len, b.subspan(2, 2), 2);
        return MEDIUM_OFFSET;
    }

    // Else: 64-bit big-endian length at b[2..9]
    b[1] = BIG_STREAM;
    to_big_endian_bytes<uint64_t>(data_len, b.subspan(2, 8), 8);
    return BIG_OFFSET;
}

int WebSocket::exit() {
    return send_request(send_buf, set_send_header(0, (1 << 7) + CONNECTION_CLOSE));
}

int WebSocket::receive_cmd(Command& cmd) {
    if (connection_closed) [[unlikely]] {
        return 0;
    }

    int err = read_stream();

    if (err < 0) {
        return -1;
    } else if (err == 1) { /* Connection closed by client*/
        return 0;
    }

    if (decode_raw_stream_cmd(cmd) < 0) {
        log<CRITICAL>("WebSocket: Cannot decode command stream\n");
        return -1;
    }

    logf<DEBUG>("[R] WebSocket: command of {} bytes\n", header.payload_size);
    return header.payload_size;
}

int WebSocket::decode_raw_stream_cmd(Command& cmd)
{
    // We need: base header up to mask, +4 mask bytes, + payload bytes
    const std::size_t need =
        static_cast<std::size_t>(header.mask_offset) + 4u +
        static_cast<std::size_t>(header.payload_size);

    if (read_str_len < need) {
        log<CRITICAL>("WebSocket: truncated masked frame\n");
        return -1;
    }

    if (header.payload_size < Command::HEADER_SIZE) {
        log<CRITICAL>("WebSocket: payload smaller than command header\n");
        return -1;
    }

    const auto* mask = reinterpret_cast<const uint8_t*>(
        read_str.data() + header.mask_offset);
    const auto* src  = reinterpret_cast<const uint8_t*>(
        read_str.data() + header.mask_offset + 4);

    // 1) decode command header
    auto* dst = reinterpret_cast<uint8_t*>(cmd.header.data());
    std::size_t k = 0; // mask index

    for (std::size_t i = 0; i < Command::HEADER_SIZE; ++i) {
        dst[i] = src[i] ^ mask[k];
        k = (k + 1) & 3;
    }

    // 2) decode payload (continue mask phase from HEADER_SIZE)
    const std::size_t payload_bytes =
        static_cast<std::size_t>(header.payload_size) - Command::HEADER_SIZE;

    if (payload_bytes > cmd.payload.size()) {
        log<CRITICAL>("WebSocket: payload longer than destination buffer\n");
        return -1;
    }

    const auto* sp = src + Command::HEADER_SIZE;
    auto* dp = reinterpret_cast<uint8_t*>(cmd.payload.data());
    k = (Command::HEADER_SIZE) & 3; // continue phase

    for (std::size_t i = 0; i < payload_bytes; ++i) {
        dp[i] = sp[i] ^ mask[k];
        k = (k + 1) & 3;
    }

    return 0;
}

int WebSocket::read_stream()
{
    reset_read_buff();

    int read_head_err = read_header();

    if (read_head_err < 0) {
        log<CRITICAL>("WebSocket: Cannot read header\n");
        return -1;
    }

    if (read_head_err == 1) {
        // Connection closed
        return read_head_err;
    }

    // Read payload
    int err = read_n_bytes(header.payload_size, header.payload_size);

    if (err < 0) {
        log<CRITICAL>("WebSocket: Cannot read payload\n");
        return -1;
    }

    return err;
}

int WebSocket::check_opcode(unsigned int opcode)
{
    switch (opcode) {
      case CONTINUATION_FRAME:
        log<CRITICAL>("WebSocket: Continuation frame is not suported\n");
        return -1;
      case TEXT_FRAME:
        break;
      case BINARY_FRAME:
        break;
      case CONNECTION_CLOSE:
        log<INFO>("WebSocket: Connection close\n");
        return 1;
      case PING:
        log<CRITICAL>("WebSocket: Ping is not suported\n");
        return -1;
      case PONG:
        log<CRITICAL>("WebSocket: Pong is not suported\n");
        return -1;
      default:
        logf<CRITICAL>("WebSocket: Invalid opcode {}\n", opcode);
        return -1;
    }

    return 0;
}

int WebSocket::read_header()
{
    if (read_n_bytes(6,6) < 0) {
        return -1;
    }

    if (connection_closed) [[unlikely]] {
        return 0;
    }

    header.fin = read_str[0] & 0x80;

    header.masked = read_str[1] & 0x80;
    unsigned char stream_size = read_str[1] & 0x7F;

    header.opcode = read_str[0] & 0x0F;

    int opcode_err = check_opcode(header.opcode);

    if (opcode_err < 0) {
        return -1;
    } else if (opcode_err == 1) {
        connection_closed = true;
        return opcode_err;
    }

    if (stream_size <= SMALL_STREAM) {
        header.header_size = SMALL_HEADER;
        header.payload_size = stream_size;
        header.mask_offset = SMALL_OFFSET;
    } else if (stream_size == MEDIUM_STREAM) {
        if (read_n_bytes(2, 2) < 0) {
            return -1;
        }

        if (connection_closed) [[unlikely]] {
            return 0;
        }

        header.header_size = MEDIUM_HEADER;
        uint16_t s = 0;
        std::memcpy(&s, reinterpret_cast<const char*>(&read_str[2]), 2);
        header.payload_size = ntohs(s);
        header.mask_offset = MEDIUM_OFFSET;
    } else if (stream_size == BIG_STREAM) {
        if (read_n_bytes(8, 8) < 0) {
            return -1;
        }

        if (connection_closed) [[unlikely]] {
            return 0;
        }

        header.header_size = BIG_HEADER;
        uint64_t l = 0;
        std::memcpy(&l, reinterpret_cast<const char*>(&read_str[2]), 8);
        header.payload_size = static_cast<int64_t>(be64toh(l));
        header.mask_offset = BIG_OFFSET;
    } else {
        log<CRITICAL>("WebSocket: Couldn't decode stream size\n");
        return -1;
    }

    if (header.payload_size > read_str.size() - 56) {
        log<CRITICAL>("WebSocket: Message too large\n");
        return -1;
    }

    return 0;
}

int WebSocket::read_n_bytes(int64_t bytes, int64_t expected) {
    int64_t remaining = bytes;
    int64_t bytes_read = -1;

    while (expected > 0) {
        while ((remaining > 0) && ((bytes_read = ::read(comm_fd, &read_str[read_str_len], remaining)) > 0)) {
            if (bytes_read > 0) {
                read_str_len += bytes_read;
                remaining -= bytes_read;
                expected -= bytes_read;
            }

            if (bytes_read < 0) {
                log<ERROR>("WebSocket: Cannot read data\n");
                return -1;
            }

            if (expected < 0) {
                expected = 0;
            }

        }

        if (bytes_read == 0) {
            log("WebSocket: Connection closed by client\n");
            connection_closed = true;
            return 1;
        }

        if (read_str_len == KOHERON_READ_STR_LEN) {
            log<CRITICAL>("WebSocket: Read buffer overflow\n");
            return -1;
        }
    }

    return 0;
}

int WebSocket::send_request(const std::string& request) {
    return send_request(reinterpret_cast<const unsigned char*>(request.c_str()), request.length());
}

int WebSocket::send_request(const unsigned char *bits, int64_t len) {
    if (connection_closed) [[unlikely]] {
        return 0;
    }

    int bytes_send = 0;
    int remaining = len;
    int offset = 0;

    while ((remaining > 0) &&
           (bytes_send = ::write(comm_fd, &bits[offset], static_cast<uint32_t>(remaining))) > 0) {
        if (bytes_send > 0) {
            offset += bytes_send;
            remaining -= bytes_send;
        }
        else if (bytes_send == 0) {
            connection_closed = true;
            log("WebSocket: Connection closed by client\n");
            return 0;
        }
    }

    if (bytes_send < 0) {
        connection_closed = true;
        logf<ERROR>("WebSocket: Cannot send request. Error {}\n", bytes_send);
        return -1;
    }

    logf<DEBUG>("[S] {} bytes\n", bytes_send);
    return bytes_send;
}

void WebSocket::reset_read_buff() {
    std::memset(read_str.data(), 0, read_str_len);
    read_str_len = 0;
}

} // namespace koheron
