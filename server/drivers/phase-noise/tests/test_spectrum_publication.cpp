#include "server/drivers/phase-noise/spectrum-publication.hpp"
#include "server/network/serializer_deserializer.hpp"
#include <atomic>
#include <cassert>
#include <fstream>
#include <thread>

int main(int argc, char** argv) {
    phase_noise::SpectrumPublication<float> publication(5);
    phase_noise::SpectrumMetadata metadata;
    metadata.state = 1; metadata.precision = 8; metadata.fs = 1e6;
    metadata.channel = 2; metadata.cic_rate = 100; metadata.navg = 3;
    metadata.count = 7; metadata.target = 0;
    metadata.lo = {10e6 + .125, 10e6, 20e6 + .25, 20e6};
    metadata.reference_clock = 2;
    publication.publish(metadata, {0.f, 0.f, -2.f, 4.f, 8.f});
    auto snapshot = publication.snapshot();
    assert(std::get<0>(snapshot) == 1);
    assert(std::get<16>(snapshot) == publication.spectrum());
    assert((publication.average_status() == std::tuple{7u, 0u}));
    if (argc > 1) {
        std::pmr::vector<unsigned char> bytes;
        net::CommandBuilder builder;
        builder.reset_into(bytes);
        builder.write_header(5, 7);
        builder.push(snapshot);
        assert(bytes.size() == 8 + 92 + 4 + 5 * sizeof(float));
        std::ofstream(argv[1], std::ios::binary).write(
            reinterpret_cast<const char*>(bytes.data()), bytes.size());
    }
    std::atomic<bool> finished{false};
    std::thread writer([&] {
        for (uint32_t i = 1; i <= 2000; ++i) {
            metadata.cic_rate = i;
            metadata.fs = 2.0 * i;
            metadata.state = i % 2; // Include invalidated captures.
            publication.publish(metadata, {float(i), -float(i)});
        }
        finished.store(true);
    });
    do {
        const auto frame = publication.snapshot();
        const auto& data = std::get<16>(frame);
        if (data.size() != 2) continue;
        assert(data[0] == float(std::get<5>(frame)));
        assert(data[1] == -data[0]);
        assert(std::get<3>(frame) == 2.0 * data[0]);
        assert(std::get<1>(frame) == std::get<5>(frame) % 2);
    } while (!finished.load());
    writer.join();
}
