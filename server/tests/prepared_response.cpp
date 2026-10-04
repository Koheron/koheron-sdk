#include "server/network/session.hpp"
#include <cstdlib>
#include <complex>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

void check(bool ok) { if (!ok) std::abort(); }

class CaptureSession : public net::Session {
  public:
    CaptureSession() : Session(net::TCP) {}
    using Session::prepare_response;
    using Session::send_prepared_response;
    std::vector<std::byte> wire;
    void shutdown() override {}
  private:
    int init_socket() override { return 0; }
    int exit_socket() override { return 0; }
    int read_command(net::Command&) override { return -1; }
    int write_bytes(std::span<const std::byte> bytes) override {
        wire.insert(wire.end(), bytes.begin(), bytes.end());
        return bytes.size();
    }
    int send_iov(std::span<const std::byte> header, std::span<const std::byte> payload, int flags) override {
        check(!(flags & MSG_ZEROCOPY));
        write_bytes(header); write_bytes(payload);
        return header.size() + payload.size();
    }
};

template<class Get, class Mutate>
void compare(Get get, Mutate mutate) {
    CaptureSession direct, prepared;
    {
        auto&& value = get();
        check(direct.send(2, 7, std::forward<decltype(value)>(value)) > 0);
    }
    auto response = [&] {
        auto&& value = get();
        return prepared.prepare_response(2, 7, std::forward<decltype(value)>(value));
    }();
    check(prepared.wire.empty()); // Preparation cannot block on network writes.
    mutate(); // Invalidate every driver-owned reference before flushing.
    check(prepared.send_prepared_response(response) > 0);
    check(direct.wire == prepared.wire);
}

int main() {
    compare([] { return uint32_t{0x12345678}; }, [] {});
    compare([] { return std::tuple{true, int32_t{-5}, 1.5f, 2.75}; }, [] {});
    compare([] { return std::complex<double>{1.0, -2.0}; }, [] {});
    std::array<uint32_t, 4> array{1, 2, 3, 4};
    auto reset = [&] { array = {1, 2, 3, 4}; };
    auto mutate_array = [&] { array.fill(99); };
    compare([&]() -> const auto& { return array; }, mutate_array); reset();
    compare([&] { return std::span<const uint32_t, 4>{array}; }, mutate_array); reset();
    compare([&] { return std::span<const uint32_t>{array}; }, mutate_array);
    compare([] { return std::array<uint32_t, 0>{}; }, [] {});
    compare([] { return std::span<const uint32_t>{}; }, [] {});
    compare([] { return std::vector<uint32_t>{}; }, [] {});
    compare([] { return std::vector<uint32_t>{1, 2, 3}; }, [] {});
    compare([] { return std::array<uint32_t, 3>{1, 2, 3}; }, [] {});
    std::vector<uint32_t> vector{1, 2, 3};
    compare([&]() -> const auto& { return vector; }, [&] { vector = std::vector<uint32_t>(100, 99); });
    std::string text(256, 'x');
    auto mutate_text = [&] { text = std::string(4096, 'y'); };
    compare([&]() -> const auto& { return text; }, mutate_text);
    text.assign(256, 'x'); compare([&] { return std::string_view{text}; }, mutate_text);
    text.assign(256, 'x'); compare([&] { return text.c_str(); }, mutate_text);
    compare([] { return "literal"; }, [] {});
    vector = {1, 2, 3}; text.assign(256, 'x');
    compare([&] {
        return std::tuple{uint32_t{5}, std::span<const uint32_t>{vector},
                          std::tuple{std::string_view{text}, text.c_str(), array}};
    }, [&] { vector = std::vector<uint32_t>(1000, 99); mutate_text(); array.fill(99); });
}
