/// Implementation of websocket.hpp
///
/// Inspired by https://github.com/MarkusPfundstein/C---Websocket-Server
///
/// (c) Koheron

#include "websocket.hpp"

#include <cstring>
#include <sstream>
#include <iostream>
#include <cstdlib>

extern "C" {
    #include <sys/socket.h> // socket definitions
    #include <arpa/inet.h>  // inet (3) funtions
    #include <sys/types.h>  // socket types
    #include <unistd.h>
}

#include "base64.hpp"
#include "sha1.h"
#include "syslog.hpp"

namespace koheron {

WebSocket::WebSocket(SysLog& syslog_)
: syslog(syslog_),
  comm_fd(-1),
  read_str_len(0),
  connection_closed(false)
{
    bzero(read_str, WEBSOCK_READ_STR_LEN);
    bzero(sha_str, 21);
}

void WebSocket::set_id(int comm_fd_)
{
    comm_fd = comm_fd_;
}

int WebSocket::authenticate()
{
    if (read_http_packet() < 0)
        return -1;

    static const std::string WSKeyIdentifier("Sec-WebSocket-Key: ");
    static const std::string WSProtocolIdentifier("Sec-WebSocket-Protocol: ");
    static const std::string WSMagic("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");

    bool is_protocol = (http_packet.find(WSProtocolIdentifier) != std::string::npos);

    std::size_t pos = http_packet.find(WSKeyIdentifier);

    if (pos == std::string::npos) {
        syslog.print<CRITICAL>("WebSocket: No WebSocket Key");
        return -1;
    }

    std::string key;
    static const int WSKeyLen = 24;
    key = http_packet.substr(pos + WSKeyIdentifier.length(), WSKeyLen);
    key.append(WSMagic);

    SHA1(reinterpret_cast<const unsigned char*>(key.c_str()), key.length(), sha_str);
    std::string final_str = base64_encode(sha_str, 20);

    std::ostringstream oss;
    oss << "HTTP/1.1 101 Switching Protocols\r\n";
    oss << "Upgrade: websocket\r\n";
    oss << "Connection: Upgrade\r\n";
    oss << "Sec-WebSocket-Accept: " << final_str << "\r\n";

    // Chrome is not happy when the Protocol wasn't
    // specified in the HTTP request:
    // "Response must not include 'Sec-WebSocket-Protocol'
    // header if not present in request: chat"
    // so only answer protocol if requested
    if (is_protocol) {
        oss << "Sec-WebSocket-Protocol: chat\r\n";
    }

    oss << "\r\n";

    return send_request(oss.str());
}

int WebSocket::read_http_packet()
{
    reset_read_buff();

    int nb_bytes_rcvd = read(comm_fd, read_str, WEBSOCK_READ_STR_LEN);

    // Check reception ...
    if (nb_bytes_rcvd < 0) {
        syslog.print<CRITICAL>("WebSocket: Read error\n");
        return -1;
    }

    if (nb_bytes_rcvd == KOHERON_READ_STR_LEN) {
        syslog.print<CRITICAL>("WebSocket: Read buffer overflow\n");
        return -1;
    }

    if (nb_bytes_rcvd == 0) { // Connection closed by client
        connection_closed = true;
        return -1;
    }

    http_packet = std::string(read_str);
    std::size_t delim_pos = http_packet.find("\r\n\r\n");

    if (delim_pos == std::string::npos) {
        syslog.print<CRITICAL>("WebSocket: No HTML header found\n");
        return -1;
    }

    syslog.print<DEBUG>("[R] HTTP header\n");
    return nb_bytes_rcvd;
}

int WebSocket::set_send_header(unsigned char *bits, int64_t data_len,
                               unsigned int format)
{
    memset(bits, 0, 10 + data_len);

    bits[0] = format;
    int mask_offset = 0;

    if (data_len <= SMALL_STREAM) {
        bits[1] = data_len;
        mask_offset = SMALL_OFFSET;
    }
    else if (data_len > SMALL_STREAM
             && data_len <= 0xFFFF) {
        bits[1] = MEDIUM_STREAM;
        bits[2] = (data_len >> 8) & 0xFF;
        bits[3] = data_len & 0xFF;
        mask_offset = MEDIUM_OFFSET;
    } else {
        bits[1] = BIG_STREAM;
        bits[2] = (data_len >> 56) & 0xFF;
        bits[3] = (data_len >> 48) & 0xFF;
        bits[4] = (data_len >> 40) & 0xFF;
        bits[5] = (data_len >> 32) & 0xFF;
        bits[6] = (data_len >> 24) & 0xFF;
        bits[7] = (data_len >> 16) & 0xFF;
        bits[8] = (data_len >>  8) & 0xFF;
        bits[9] = data_len & 0xFF;
        mask_offset = BIG_OFFSET;
    }

    return mask_offset;
}

int WebSocket::exit()
{
    return send_request(send_buf, set_send_header(send_buf, 0, (1 << 7) + CONNECTION_CLOSE));
}

int WebSocket::receive_cmd(Command& cmd)
{
    if (connection_closed)
        return 0;

    int err = read_stream();

    if (err < 0)
        return -1;
    else if (err == 1) /* Connection closed by client*/
        return 0;

    if (decode_raw_stream_cmd(cmd) < 0) {
        syslog.print<CRITICAL>("WebSocket: Cannot decode command stream\n");
        return -1;
    }

    syslog.print<DEBUG>("[R] WebSocket: command of %u bytes\n", header.payload_size);
    return header.payload_size;
}

int WebSocket::decode_raw_stream_cmd(Command& cmd)
{
    if (read_str_len < header.header_size + 1)
        return -1;

    char *mask = read_str + header.mask_offset;
    char *payload_ptr = read_str + header.mask_offset + 4;

    for (int64_t i = 0; i < Command::HEADER_SIZE; ++i) {
        cmd.header.data()[i] = (payload_ptr[i] ^ mask[i % 4]);
    }

    for (int64_t i = Command::HEADER_SIZE; i < header.payload_size; ++i) {
        cmd.payload.data()[i - Command::HEADER_SIZE] = (payload_ptr[i] ^ mask[i % 4]);
    }

    return 0;
}

int WebSocket::decode_raw_stream()
{
    if (read_str_len < header.header_size + 1)
        return -1;

    char *mask = read_str + header.mask_offset;
    payload = read_str + header.mask_offset + 4;

    for (int64_t i = 0; i < header.payload_size; ++i) {
        payload[i] = (payload[i] ^ mask[i % 4]);
    }

    return 0;
}

int WebSocket::read_stream()
{
    reset_read_buff();

    int read_head_err = read_header();

    if (read_head_err < 0) {
        syslog.print<CRITICAL>("WebSocket: Cannot read header\n");
        return -1;
    }

    if (read_head_err == 1) {
        // Connection closed
        return read_head_err;
    }

    // Read payload
    int err = read_n_bytes(header.payload_size, header.payload_size);

    if (err < 0) {
        syslog.print<CRITICAL>("WebSocket: Cannot read payload\n");
        return -1;
    }

    return err;
}

int WebSocket::check_opcode(unsigned int opcode)
{
    switch (opcode) {
      case CONTINUATION_FRAME:
        syslog.print<CRITICAL>("WebSocket: Continuation frame is not suported\n");
        return -1;
      case TEXT_FRAME:
        break;
      case BINARY_FRAME:
        break;
      case CONNECTION_CLOSE:
        syslog.print<INFO>("WebSocket: Connection close\n");
        return 1;
      case PING:
        syslog.print<CRITICAL>("WebSocket: Ping is not suported\n");
        return -1;
      case PONG:
        syslog.print<CRITICAL>("WebSocket: Pong is not suported\n");
        return -1;
      default:
        syslog.print<CRITICAL>("WebSocket: Invalid opcode %u\n", opcode);
        return -1;
    }

    return 0;
}

int WebSocket::read_header()
{
    if (read_n_bytes(6,6) < 0)
        return -1;

    if (connection_closed)
        return 0;

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
    }
    else if (stream_size == MEDIUM_STREAM) {
        if (read_n_bytes(2, 2) < 0) {
            return -1;
        }
        if (connection_closed) {
            return 0;
        }
        header.header_size = MEDIUM_HEADER;
        uint16_t s = 0;
        memcpy(&s, reinterpret_cast<const char*>(&read_str[2]), 2);
        header.payload_size = ntohs(s);
        header.mask_offset = MEDIUM_OFFSET;
    }
    else if (stream_size == BIG_STREAM) {
        if (read_n_bytes(8, 8) < 0) {
            return -1;
        }
        if (connection_closed) {
            return 0;
        }
        header.header_size = BIG_HEADER;
        uint64_t l = 0;
        memcpy(&l, reinterpret_cast<const char*>(&read_str[2]), 8);
        header.payload_size = static_cast<int64_t>(be64toh(l));
        header.mask_offset = BIG_OFFSET;
    } else {
        syslog.print<CRITICAL>("WebSocket: Couldn't decode stream size\n");
        return -1;
    }

    if (header.payload_size > WEBSOCK_READ_STR_LEN - 56) {
        syslog.print<CRITICAL>("WebSocket: Message too large\n");
        return -1;
    }

    return 0;
}

int WebSocket::read_n_bytes(int64_t bytes, int64_t expected)
{
    int64_t remaining = bytes;
    int64_t bytes_read = -1;

    while (expected > 0) {
        while ((remaining > 0) && ((bytes_read = read(comm_fd, &read_str[read_str_len], remaining)) > 0)) {

            if (bytes_read > 0) {
                read_str_len += bytes_read;
                remaining -= bytes_read;
                expected -= bytes_read;
            }

            if (bytes_read < 0) {
                syslog.print<ERROR>("WebSocket: Cannot read data\n");
                return -1;
            }

            if (expected < 0) {
                expected = 0;
            }

        }

        if (bytes_read == 0) {
            syslog.print<INFO>("WebSocket: Connection closed by client\n");
            connection_closed = true;
            return 1;
        }

        if (read_str_len == KOHERON_READ_STR_LEN) {
            syslog.print<CRITICAL>("WebSocket: Read buffer overflow\n");
            return -1;
        }
    }

    return 0;
}

int WebSocket::send_request(const std::string& request)
{
    return send_request(reinterpret_cast<const unsigned char*>(request.c_str()), request.length());
}

int WebSocket::send_request(const unsigned char *bits, int64_t len)
{
    if (connection_closed)
        return 0;

    int bytes_send = 0;
    int remaining = len;
    int offset = 0;

    while ((remaining > 0) &&
           (bytes_send = write(comm_fd, &bits[offset], static_cast<uint32_t>(remaining))) > 0) {
        if (bytes_send > 0) {
            offset += bytes_send;
            remaining -= bytes_send;
        }
        else if (bytes_send == 0) {
            connection_closed = true;
            syslog.print<INFO>("WebSocket: Connection closed by client\n");
            return 0;
        }
    }

    if (bytes_send < 0) {
        connection_closed = true;
        syslog.print<ERROR>(
                "WebSocket: Cannot send request. Error %i\n", bytes_send);
        return -1;
    }

    syslog.print<DEBUG>("[S] %i bytes\n", bytes_send);
    return bytes_send;
}

void WebSocket::reset_read_buff()
{
    std::memset(read_str, 0, read_str_len);
    read_str_len = 0;
}

} // namespace koheron
