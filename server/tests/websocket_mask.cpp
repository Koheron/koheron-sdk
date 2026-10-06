// Exercise the production unmasker, including exact buffer ends and wire frames.
#include "server/network/websocket.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>
#include <sys/mman.h>
#include <sys/socket.h>
#include <unistd.h>

void check(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

void boundaries() {
    const std::array<std::array<uint8_t, 4>, 3> masks{{
        {0, 0, 0, 0}, {0xff, 0xff, 0xff, 0xff}, {0x17, 0x81, 0x00, 0xe3}}};
    for (std::size_t size : {0u, 1u, 3u, 4u, 15u, 16u, 17u, 31u, 32u,
                            63u, 64u, 65u, 79u, 80u, 81u, 125u, 126u,
                            127u, 255u, 256u, 4095u, 4096u, 65535u, 65536u,
                            262088u}) {
        for (const auto& mask : masks) for (std::size_t phase = 0; phase < 4; ++phase) {
            for (std::size_t alignment = 0; alignment < 16; ++alignment) {
                std::vector<uint8_t> src(size + 32, 0xa5), dst(size + 32, 0xcc);
                auto* sp = src.data() + alignment;
                auto* dp = dst.data() + 15 - alignment;
                for (std::size_t i = 0; i < size; ++i) sp[i] = (31 * i + 17) & 255;
                const auto original = src;
                net::detail::unmask(sp, dp, size, mask.data(), phase);
                for (std::size_t i = 0; i < dst.size(); ++i) {
                    const auto offset = i - (15 - alignment);
                    const uint8_t expected = offset < size
                        ? sp[offset] ^ mask[(offset + phase) & 3] : 0xcc;
                    check(dst[i] == expected, "wrong output or overwritten sentinel");
                }
                check(src == original, "source modified");
                net::detail::unmask(sp, sp, size, mask.data(), phase);
                check(std::equal(sp, sp + size, dp), "in-place result differs");
                net::detail::unmask_scalar(sp, sp, size, mask.data(), phase);
                check(src == original, "scalar fallback differs");
            }
        }
    }
}

struct Guarded {
    std::size_t page = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
    uint8_t* data = static_cast<uint8_t*>(mmap(nullptr, 2 * page,
        PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    Guarded() {
        check(data != MAP_FAILED, "mmap failed");
        check(mprotect(data + page, page, PROT_NONE) == 0, "mprotect failed");
    }
    ~Guarded() { munmap(data, 2 * page); }
};

void guard_pages() {
    Guarded source, destination, key;
    auto* mask = key.data + key.page - 4;
    mask[0] = 0x17; mask[1] = 0x81; mask[2] = 0; mask[3] = 0xe3;
    // Every tail and unaligned address, with inaccessible bytes immediately after it.
    for (std::size_t size = 0; size <= 160; ++size) {
        auto* src = source.data + source.page - size;
        auto* dst = destination.data + destination.page - size;
        for (std::size_t phase = 0; phase < 4; ++phase) {
            std::fill(src, src + size, 0xa5);
            net::detail::unmask(src, dst, size, mask, phase);
            for (std::size_t i = 0; i < size; ++i)
                check(dst[i] == (0xa5 ^ mask[(i + phase) & 3]), "guarded output differs");
        }
    }
}

template<std::size_t HeaderSize>
void receive_case(std::size_t size, std::size_t split) {
    int fd[2];
    check(socketpair(AF_UNIX, SOCK_STREAM, 0, fd) == 0, "socketpair failed");
    std::vector<uint8_t> payload(HeaderSize + size);
    for (std::size_t i = 0; i < payload.size(); ++i) payload[i] = (31 * i + 17) & 255;
    std::vector<uint8_t> frame{0x82};
    const auto length = payload.size();
    if (length < 126) frame.push_back(0x80 | length);
    else if (length < 65536) {
        frame.insert(frame.end(), {0xfe, static_cast<uint8_t>(length >> 8), static_cast<uint8_t>(length)});
    } else {
        frame.push_back(0xff);
        for (int shift = 56; shift >= 0; shift -= 8)
            frame.push_back(static_cast<uint8_t>(static_cast<uint64_t>(length) >> shift));
    }
    const uint8_t mask[]{0x17, 0x81, 0, 0xe3};
    frame.insert(frame.end(), mask, mask + 4);
    for (std::size_t i = 0; i < length; ++i) frame.push_back(payload[i] ^ mask[i & 3]);
    bool sent = true;
    std::thread writer([&] {
        std::size_t pos = 0;
        while (pos < frame.size()) {
            auto n = ::send(fd[1], frame.data() + pos,
                            std::min(split, frame.size() - pos), MSG_NOSIGNAL);
            if (n <= 0) { sent = false; break; }
            pos += n;
        }
        ::shutdown(fd[1], SHUT_WR);
    });
    net::WebSocket ws;
    ws.set_id(fd[0]);
    net::Buffer<HeaderSize> header;
    net::Buffer<262144> body;
    const auto received = ws.receive_cmd(header, body);
    writer.join();
    ::close(fd[0]); ::close(fd[1]);
    check(sent && received == static_cast<int>(length), "receive failed");
    check(std::equal(payload.begin(), payload.begin() + HeaderSize,
                     reinterpret_cast<const uint8_t*>(header.data())), "header mismatch");
    check(std::equal(payload.begin() + HeaderSize, payload.end(),
                     reinterpret_cast<const uint8_t*>(body.data())), "body mismatch");
}

void receive() {
    for (std::size_t size : {0u, 1u, 63u, 64u, 65u, 117u, 118u, 65527u, 65528u, 262000u}) {
        receive_case<8>(size, 4093);
        receive_case<11>(size, 4093); // Mask phase continues across the command header.
    }
    receive_case<8>(81, 1); // Fragmented socket reads, independent of WebSocket framing.
}

int main(int argc, char** argv) {
    try {
        check(argc == 2, "expected test name");
        const std::string test = argv[1];
        if (test == "boundaries") boundaries();
        else if (test == "guard-pages") guard_pages();
        else if (test == "receive") receive();
        else throw std::runtime_error("unknown test");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
