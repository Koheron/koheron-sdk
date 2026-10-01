/// Websocket protocol interface
///
/// (c) Koheron

#ifndef __WEBSOCKET_HPP__
#define __WEBSOCKET_HPP__

#include "server/runtime/syslog.hpp"
#include "server/network/configs/server_definitions.hpp"
#include "server/network/buffer.hpp"

#include <array>
#include <string>
#include <ranges>
#include <cstring>
#include <string_view>

namespace net {

struct Command;

class WebSocket
{
  public:
    WebSocket();

    void set_id(int comm_fd_);
    int authenticate();

    template<uint32_t HEADER_SIZE, uint32_t CMD_PAYLOAD_BUFFER_LEN>
    int receive_cmd(Buffer<HEADER_SIZE>& cmd_header,
                    Buffer<CMD_PAYLOAD_BUFFER_LEN>& cmd_payload);

    /// Send binary blob
    template<std::ranges::contiguous_range R>
    int send(const R& r);

    template<std::ranges::contiguous_range Rh,
             std::ranges::contiguous_range Rp>
    int send(const Rh& h, const Rp& p);

    int64_t payload_size() const {return header.payload_size;}

    bool is_closed() const {return connection_closed;}

    int exit();

  private:
    int comm_fd;

    // Buffers
    uint32_t read_str_len;
    std::array<char, WEBSOCK_READ_STR_LEN> read_str{};

    unsigned char sha_str[21];
    std::array<unsigned char, WEBSOCK_SEND_BUF_LEN> send_buf{};

    void reset_read_buff();

    std::string_view http_packet;

    struct {
        unsigned int header_size;
        int mask_offset;
        int64_t payload_size;
        bool fin;
        bool masked;
        unsigned char opcode;
        unsigned char res[3];
    } header;

    bool connection_closed;

    enum OpCode {
        CONTINUATION_FRAME = 0x0,
        TEXT_FRAME         = 0x1,
        BINARY_FRAME       = 0x2,
        CONNECTION_CLOSE   = 0x8,
        PING               = 0x9,
        PONG               = 0xA
    };

    enum StreamSize {
        SMALL_STREAM  = 125,
        MEDIUM_STREAM = 126,
        BIG_STREAM    = 127
    };

    enum HeaderSize {
        SMALL_HEADER  = 6,
        MEDIUM_HEADER = 8,
        BIG_HEADER    = 14
    };

    enum MaskOffset {
        SMALL_OFFSET  = 2,
        MEDIUM_OFFSET = 4,
        BIG_OFFSET    = 10
    };

    // Internal functions
    int read_http_packet();

    template<uint32_t HEADER_SIZE, uint32_t PAYLOAD_SIZE>
    int decode_raw_stream_cmd(Buffer<HEADER_SIZE>& cmd_header,
                              Buffer<PAYLOAD_SIZE>& cmd_payload);
    int read_stream();
    int read_header();
    int check_opcode(unsigned int opcode);
    int read_n_bytes(int64_t bytes, int64_t expected);

    int set_send_header(int64_t data_len, unsigned int format);

    template<std::ranges::contiguous_range R>
    int send_request(const R& r, int64_t len);
    int send_request(const std::string& request);
    int send_request(const unsigned char *bits, int64_t len);
};

template<std::ranges::contiguous_range R>
int WebSocket::send_request(const R& r, int64_t len) {
    return send_request(reinterpret_cast<const unsigned char*>(std::data(r)), len);
}

template<std::ranges::contiguous_range R>
int WebSocket::send(const R& r) {
    using T = std::remove_cvref_t<std::ranges::range_value_t<R>>;

    if (connection_closed) {
        return 0;
    }

    auto char_data_len = std::size(r) * sizeof(T) / sizeof(char);

    if (char_data_len + 10 > send_buf.size()) {
        return -1;
    }

    auto mask_offset = set_send_header(char_data_len, (1 << 7) + BINARY_FRAME);
    std::memcpy(&send_buf[mask_offset], std::data(r), char_data_len);
    return send_request(send_buf, static_cast<int64_t>(mask_offset) + static_cast<int64_t>(char_data_len));
}

template<std::ranges::contiguous_range Rh,
         std::ranges::contiguous_range Rp>
int WebSocket::send(const Rh& h, const Rp& p) {
    using H = std::remove_cvref_t<std::ranges::range_value_t<Rh>>;
    using P = std::remove_cvref_t<std::ranges::range_value_t<Rp>>;

    if (connection_closed) {
        return 0;
    }

    // bytes lengths
    const std::size_t hbytes = std::size(h) * sizeof(H);
    const std::size_t pbytes = std::size(p) * sizeof(P);
    const std::size_t total  = hbytes + pbytes;

    // Heuristic max header size allowance you already use (+10)
    // (Server frames are unmasked; 10 bytes covers max server header)
    constexpr std::size_t MAX_WS_HDR = 10;

    // ---- Fast path: one frame (FIN=1, opcode=BINARY) ----
    if (total + MAX_WS_HDR <= send_buf.size()) {
        const auto payload_off = set_send_header(static_cast<int64_t>(total),
                                                 (1 << 7) + BINARY_FRAME); // FIN=1 | BINARY
        // copy header then payload into the frame
        std::memcpy(send_buf.data() + payload_off, std::data(h), hbytes);
        std::memcpy(send_buf.data() + payload_off + hbytes, std::data(p), pbytes);

        return send_request(send_buf,
                            static_cast<int64_t>(payload_off) +
                            static_cast<int64_t>(total));
    }

    // ---- Fallback: fragmented send (no heap, no extra copies) ----
    // First fragment: application header only (FIN=0, opcode=BINARY)
    if (hbytes + MAX_WS_HDR > send_buf.size()) {
        // Extremely large "application header" (unusual): chunk it too.
        // Send as first fragment(s) with opcode=BINARY then CONTINUATION.
        // 1) First chunk as BINARY, FIN=0
        std::size_t sent = 0;
        {
            const std::size_t chunk = send_buf.size() - MAX_WS_HDR;
            const std::size_t sz    = std::min(hbytes, chunk);
            const auto off = set_send_header(static_cast<int64_t>(sz), BINARY_FRAME); // FIN=0
            std::memcpy(send_buf.data() + off,
                        reinterpret_cast<const unsigned char*>(std::data(h)), sz);
            const int n = send_request(send_buf, static_cast<int64_t>(off) + static_cast<int64_t>(sz));
            if (n <= 0) return n;
            sent += sz;
        }
        // 2) Remaining header as CONTINUATION fragments, FIN=0 (until last)
        while (sent < hbytes) {
            const std::size_t chunk = send_buf.size() - MAX_WS_HDR;
            const std::size_t sz    = std::min(hbytes - sent, chunk);
            // If there is payload following or more header remains after this, FIN=0
            const bool fin = (sent + sz == hbytes) && (pbytes == 0);
            const auto fmt = (fin ? (1 << 7) : 0) + CONTINUATION_FRAME;
            const auto off = set_send_header(static_cast<int64_t>(sz), fmt);
            std::memcpy(send_buf.data() + off,
                        reinterpret_cast<const unsigned char*>(std::data(h)) + sent, sz);
            const int n = send_request(send_buf, static_cast<int64_t>(off) + static_cast<int64_t>(sz));
            if (n <= 0) return n;
            sent += sz;
        }
    } else {
        // Header fits → single first fragment contains the app header
        const auto off = set_send_header(static_cast<int64_t>(hbytes), BINARY_FRAME); // FIN=0
        std::memcpy(send_buf.data() + off, std::data(h), hbytes);
        const int n = send_request(send_buf, static_cast<int64_t>(off) + static_cast<int64_t>(hbytes));
        if (n <= 0) return n;
    }

    // Continuation fragments for the payload (FIN=1 on the last one)
    std::size_t psent = 0;
    while (psent < pbytes) {
        const std::size_t chunk = send_buf.size() - MAX_WS_HDR;
        const std::size_t sz    = std::min(pbytes - psent, chunk);
        const bool last         = (psent + sz == pbytes);
        const auto fmt          = (last ? (1 << 7) : 0) + CONTINUATION_FRAME; // FIN on last

        const auto off = set_send_header(static_cast<int64_t>(sz), fmt);
        std::memcpy(send_buf.data() + off,
                    reinterpret_cast<const unsigned char*>(std::data(p)) + psent, sz);

        const int n = send_request(send_buf, static_cast<int64_t>(off) + static_cast<int64_t>(sz));
        if (n <= 0) return n;
        psent += sz;
    }

    return static_cast<int>(total);
}

template<uint32_t HEADER_SIZE, uint32_t CMD_PAYLOAD_BUFFER_LEN>
int WebSocket::receive_cmd(Buffer<HEADER_SIZE>& cmd_header,
                           Buffer<CMD_PAYLOAD_BUFFER_LEN>& cmd_payload) {
    if (connection_closed) [[unlikely]] {
        return 0;
    }

    int err = read_stream();

    if (err < 0) {
        return -1;
    } else if (err == 1) { /* Connection closed by client*/
        return 0;
    }

    if (decode_raw_stream_cmd(cmd_header, cmd_payload) < 0) {
        log<CRITICAL>("WebSocket: Cannot decode command stream\n");
        return -1;
    }

    logf<DEBUG>("[R] WebSocket: command of {} bytes\n", header.payload_size);
    return header.payload_size;
}

template<uint32_t HEADER_SIZE, uint32_t PAYLOAD_SIZE>
int WebSocket::decode_raw_stream_cmd(Buffer<HEADER_SIZE>& cmd_header,
                                     Buffer<PAYLOAD_SIZE>& cmd_payload) {
    // We need: base header up to mask, +4 mask bytes, + payload bytes
    const std::size_t need =
        static_cast<std::size_t>(header.mask_offset) + 4u +
        static_cast<std::size_t>(header.payload_size);

    if (read_str_len < need) {
        log<CRITICAL>("WebSocket: truncated masked frame\n");
        return -1;
    }

    if (header.payload_size < HEADER_SIZE) {
        log<CRITICAL>("WebSocket: payload smaller than command header\n");
        return -1;
    }

    const auto* mask = reinterpret_cast<const uint8_t*>(
        read_str.data() + header.mask_offset);
    const auto* src  = reinterpret_cast<const uint8_t*>(
        read_str.data() + header.mask_offset + 4);

    // 1) decode command header
    auto* dst = reinterpret_cast<uint8_t*>(cmd_header.data());
    std::size_t k = 0; // mask index

    for (std::size_t i = 0; i < HEADER_SIZE; ++i) {
        dst[i] = src[i] ^ mask[k];
        k = (k + 1) & 3;
    }

    // 2) decode payload (continue mask phase from HEADER_SIZE)
    const std::size_t payload_bytes =
        static_cast<std::size_t>(header.payload_size) - HEADER_SIZE;

    if (payload_bytes > cmd_payload.size()) {
        log<CRITICAL>("WebSocket: payload longer than destination buffer\n");
        return -1;
    }

    const auto* sp = src + HEADER_SIZE;
    auto* dp = reinterpret_cast<uint8_t*>(cmd_payload.data());
    k = (HEADER_SIZE) & 3; // continue phase

    for (std::size_t i = 0; i < payload_bytes; ++i) {
        dp[i] = sp[i] ^ mask[k];
        k = (k + 1) & 3;
    }

    return 0;
}

} // namespace net

#endif // __WEBSOCKET_HPP__
