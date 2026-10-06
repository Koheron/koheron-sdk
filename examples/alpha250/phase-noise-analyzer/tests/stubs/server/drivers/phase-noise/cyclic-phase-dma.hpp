#pragma once
#include "server/drivers/dma-s2mm.hpp"
#include "server/runtime/driver_manager.hpp"
#include <optional>
#include <atomic>
#include <array>
class CyclicPhaseDma {
    DmaS2MM& engine = rt::get_driver<DmaS2MM>();
    std::atomic<uint64_t> epoch{0};
    bool started = false;
    std::atomic<uint64_t> completed{0};
public:
    static constexpr uint32_t samples_per_chunk = 8192;
    uint64_t overruns() const { return 0; }
    template<uint32_t N> struct Snapshot {
        std::array<int32_t, N> samples;
        uint64_t end_chunk, generation, skipped_hops = 0;
        uint32_t precision;
        bool overflow, sample_gap, mixed_precision;
        bool matches_precision(uint32_t bits) const { return !mixed_precision && precision == bits; }
    };
    template<class Frequency, class Apply> void configure_sampling(Frequency, Apply&& apply) {
        // A configuration aborts the in-flight epoch, never waits for its duration.
        hw::dma_in_flight.store(false);
        apply();
        ++epoch;
    }
    uint64_t completed_chunks() const { return completed.load(); }
    uint64_t generation() const { return epoch.load(); }
    void start_acquisition() { started = true; }
    template<uint32_t N> std::optional<Snapshot<N>> read(uint64_t, const std::atomic<bool>& running, uint32_t = 0, bool = false) {
        if (!started || !running.load()) return std::nullopt;
        const auto generation = epoch.load();
        engine.start_transfer<mem::ram, prm::n_pts, int32_t>();
        if (!engine.wait_for_transfer_checked(0)) return std::nullopt;
        const auto flags = hw::get_memory<mem::status>().read<reg::phase_packet>();
        Snapshot<N> snapshot;
        snapshot.samples = hw::get_memory<mem::ram>().read_array<int32_t, N, 98304 * sizeof(int32_t)>();
        snapshot.end_chunk = (completed += N / 8192);
        snapshot.generation = generation;
        snapshot.precision = flags & 0xfu;
        snapshot.overflow = flags & 0x10u;
        snapshot.sample_gap = flags & 0x40u;
        snapshot.mixed_precision = flags & 0x20u;
        return snapshot;
    }
};
