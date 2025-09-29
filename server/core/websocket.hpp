/// Websocket protocol interface
///
/// (c) Koheron

#ifndef __WEBSOCKET_HPP__
#define __WEBSOCKET_HPP__

#include "server/core/configs/server_definitions.hpp"
#include "server/core/configs/config.hpp"
#include "server/core/commands.hpp"

#include <array>
#include <string>
#include <ranges>

namespace koheron {

class WebSocket
{
  public:
    WebSocket();

    void set_id(int comm_fd_);
    int authenticate();
    int receive_cmd(Command& cmd);

    /// Send binary blob
    template<std::ranges::contiguous_range R>
    int send(const R& r);

    char* get_payload_no_copy() {return payload;}
    int64_t payload_size() const {return header.payload_size;}

    bool is_closed() const {return connection_closed;}

    int exit();

  private:
    int comm_fd;

    // Buffers
    uint32_t read_str_len;
    std::array<char, WEBSOCK_READ_STR_LEN> read_str{};

    char *payload;
    unsigned char sha_str[21];
    std::array<unsigned char, WEBSOCK_SEND_BUF_LEN> send_buf{};

    void reset_read_buff();

    std::string http_packet;

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
    int decode_raw_stream();
    int decode_raw_stream_cmd(Command& cmd);
    int read_stream();
    int read_header();
    int check_opcode(unsigned int opcode);
    int read_n_bytes(int64_t bytes, int64_t expected);

    int set_send_header(unsigned char *bits, int64_t data_len,
                        unsigned int format);
    int send_request(const std::string& request);
    int send_request(const unsigned char *bits, int64_t len);
};

template<std::ranges::contiguous_range R>
inline int WebSocket::send(const R& r)
{
    using T = std::remove_cvref_t<std::ranges::range_value_t<R>>;

    if (connection_closed) {
        return 0;
    }

    auto char_data_len = std::size(r) * sizeof(T) / sizeof(char);

    if (char_data_len + 10 > send_buf.size()) {
        return -1;
    }

    auto mask_offset = set_send_header(send_buf.data(), char_data_len, (1 << 7) + BINARY_FRAME);
    std::memcpy(&send_buf[mask_offset], std::data(r), char_data_len);
    return send_request(send_buf.data(), static_cast<int64_t>(mask_offset) + static_cast<int64_t>(char_data_len));
}

} // namespace koheron

#endif // __WEBSOCKET_HPP__
