#pragma once

#include <array>
#include <cstdint>
#include <mutex>
#include <shared_mutex>
#include <tuple>
#include <vector>

namespace phase_noise {

// Describes the samples which produced a spectrum, rather than live controls.
struct SpectrumMetadata {
    uint32_t state = 0, precision = 0;
    double fs = 0.0;
    uint32_t channel = 0, cic_rate = 0, navg = 1, count = 0, target = 1;
    std::array<double, 4> lo{};
    uint32_t mode = 0;
    double delay = 0.0;
    uint32_t reference_clock = 0;
};

template<class Density>
class SpectrumPublication {
    mutable std::shared_mutex mutex;
    uint64_t sequence = 0;
    SpectrumMetadata metadata;
    std::vector<Density> density;

  public:
    explicit SpectrumPublication(std::size_t bins) : density(bins) {}

    // Acquisition/settings writers own their processing lock before publishing.
    void publish(SpectrumMetadata next, std::vector<Density> values) {
        std::unique_lock lock(mutex);
        metadata = next;
        density = std::move(values);
        ++sequence;
    }

    auto settings() const {
        std::shared_lock lock(mutex);
        return metadata;
    }

    auto average_status() const {
        std::shared_lock lock(mutex);
        return std::tuple{metadata.count, metadata.target};
    }

    auto spectrum() const {
        std::shared_lock lock(mutex);
        return density;
    }

    auto snapshot() const {
        std::shared_lock lock(mutex);
        // 92 network-endian metadata bytes, then a byte-length-prefixed native
        // float vector. The wire shape is common to all three board designs.
        const auto& m = metadata;
        return std::tuple{sequence, m.state, m.precision, m.fs, m.channel,
            m.cic_rate, m.navg, m.count, m.target,
            m.lo[0], m.lo[1], m.lo[2], m.lo[3], m.mode, m.delay,
            m.reference_clock, density};
    }
};

} // namespace phase_noise
