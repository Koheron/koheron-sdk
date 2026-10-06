/// Continuous single-stream or synchronized paired phase acquisition. Hardware fills a cyclic SG DMA
/// ring; readers copy fresh windows and reject overwritten or damaged samples.
#ifndef PNA_PHASE_RING_HPP
#define PNA_PHASE_RING_HPP

#include <atomic>
#include <array>
#include <optional>
#include <mutex>
#include <algorithm>
#include <thread>
#include <chrono>
#include <vector>
#include <scicpp/core.hpp>
#include "server/hardware/memory_manager.hpp"
#include "packet-framer.hpp"
#include "acquisition-window.hpp"

template<uint32_t Size> struct PhaseRingSnapshotBase {
    uint64_t end_chunk = 0, generation = 0, skipped_hops = 0;
    bool overflow = false, mixed_precision = false, sample_gap = false;
};
template<uint32_t Size, uint32_t Streams> struct PhaseRingSnapshot;
template<uint32_t Size> struct PhaseRingSnapshot<Size, 1> : PhaseRingSnapshotBase<Size> {
    std::array<int32_t, Size> samples;
    uint32_t precision = 0;
    auto& data(uint32_t) { return samples; }
    auto& bits(uint32_t) { return precision; }
    bool matches_precision(uint32_t requested) const {
        return !this->mixed_precision && precision == requested;
    }
};
template<uint32_t Size> struct PhaseRingSnapshot<Size, 2> : PhaseRingSnapshotBase<Size> {
    std::array<int32_t, Size> x, y;
    uint32_t precision_x = 0, precision_y = 0;
    auto& data(uint32_t channel) { return channel == 0 ? x : y; }
    auto& bits(uint32_t channel) { return channel == 0 ? precision_x : precision_y; }
    bool matches_precision(uint32_t requested) const {
        return !this->mixed_precision && precision_x == requested && precision_y == requested;
    }
};

template<uint32_t Streams>
class BasicCyclicPhaseDma {
    static_assert(Streams == 1 || Streams == 2);
    using Frequency = scicpp::units::frequency<float>;
  public:
    static constexpr uint32_t samples_per_chunk = 8192;
    static constexpr uint32_t ring_chunks = 512;
    static constexpr uint32_t withheld_packets = 2;
    static constexpr uint32_t descriptor_bytes = 64;
    static constexpr uint32_t descriptors = Streams * ring_chunks;
    static constexpr uint32_t payload_offset = descriptors * descriptor_bytes;
    static constexpr uint32_t chunk_bytes = samples_per_chunk * sizeof(int32_t);
    static_assert(payload_offset + descriptors * chunk_bytes <= hw::Memory<mem::ram>::size);

    BasicCyclicPhaseDma() {
        ctl.write<reg::acquisition_run>(0);
        hp.set_bit<0x0, 0>();
        hp.set_bit<0x14, 0>();
    }
    ~BasicCyclicPhaseDma() {
        std::lock_guard lock(transfer_mtx);
        stop_ring();
    }

    template<typename Apply>
    void configure_sampling(Frequency frequency, Apply&& apply) {
        std::lock_guard lock(transfer_mtx);
        const bool restart = acquisition_requested;
        update_progress();
        stop_ring();
        sampling = frequency;
        apply();
        if (restart) acquisition_started.store(start_ring());
    }
    void start_acquisition() {
        std::lock_guard lock(transfer_mtx);
        acquisition_requested = true;
        if (!acquisition_started.load()) acquisition_started.store(start_ring());
    }
    uint64_t generation() const {
        std::lock_guard lock(transfer_mtx);
        return ring_generation;
    }
    uint64_t completed_chunks() const {
        std::lock_guard lock(transfer_mtx);
        update_progress();
        return write_count;
    }

    void set_fs(Frequency frequency) { sampling = frequency; }
    template<uint32_t data_size> using Snapshot = PhaseRingSnapshot<data_size, Streams>;
    uint64_t overruns() const {
        std::lock_guard lock(transfer_mtx);
        return ring_overruns;
    }

    template<uint32_t data_size>
    std::optional<Snapshot<data_size>> read(uint64_t consumed, const std::atomic<bool>& keep_running,
                                            uint32_t hop = 0, bool initialized = false) {
        static_assert(data_size > 0 && data_size % samples_per_chunk == 0);
        constexpr uint32_t chunks = data_size / samples_per_chunk;
        static_assert(chunks + withheld_packets + 1 < ring_chunks);
        uint64_t skipped_hops = 0;
        while (keep_running.load()) {
            {
                std::lock_guard lock(transfer_mtx);
                update_progress();
                // A configuration change temporarily stops the ring while
                // holding this mutex. Observe its final state only after the
                // restart, rather than mistaking that pause for a DMA failure.
                if (!acquisition_started.load()) return std::nullopt;
                const uint64_t lower = std::max(consumed, generation_base + 1);
                // A reset observed during the wait must refill an entire window.
                const bool same_epoch = initialized && consumed > generation_base;
                auto window = hop ? streaming_acquisition_window(write_count, lower, chunks, hop, same_epoch)
                                  : acquisition_window(write_count, lower, chunks);
                if (window) {
                    if (hop && !acquisition_window_is_intact(*window, write_count, ring_chunks - 3)) {
                        // Consumer lag does not damage the producer or the valid
                        // spectra already averaged. Resume at the newest complete
                        // window on the same hop grid, explicitly reporting loss.
                        if (!skipped_hops) ++ring_overruns;
                        const uint64_t skipped = (write_count - window->end_chunk) / hop;
                        skipped_hops += skipped;
                        window->end_chunk += skipped * hop;
                        window->first_chunk = window->end_chunk - chunks;
                    }
                    Snapshot<data_size> snapshot;
                    snapshot.end_chunk = window->end_chunk;
                    snapshot.generation = ring_generation;
                    snapshot.skipped_hops = skipped_hops;
                    const uint32_t retained = hop && initialized && cache_generation == ring_generation &&
                        cache_end == consumed && cached[0].size() == data_size &&
                        snapshot.end_chunk - consumed < chunks ?
                        chunks - uint32_t(snapshot.end_chunk - consumed) : 0;
                    for (uint32_t channel = 0; channel < Streams; ++channel) {
                        if (retained) std::copy(cached[channel].end() - retained * samples_per_chunk,
                            cached[channel].end(), snapshot.data(channel).begin());
                    }
                    barrier();
                    bool complete = true;
                    for (uint32_t j = 0; j < chunks; ++j) {
                        const auto chunk = window->first_chunk + j;
                        const uint32_t slot = chunk % ring_chunks;
                        for (uint32_t channel = 0; channel < Streams; ++channel) {
                            const auto metadata = mux.get_queued_status(
                                (Streams * (chunk - generation_base) + channel) % 1024);
                            const auto status = dma_descriptor_status(Streams * slot + channel);
                            complete &= (status & 0xF3FFFFFFu) == (0x80000000u | chunk_bytes);
                            const uint32_t bits = metadata & 0xfu;
                            auto& precision = snapshot.bits(channel);
                            if (j == 0) precision = bits;
                            snapshot.overflow |= (metadata & 0x10u) != 0;
                            snapshot.sample_gap |= (metadata & 0x40u) != 0;
                            snapshot.mixed_precision |= (metadata & 0x20u) != 0 || bits != precision;
                            if (j >= retained) {
                                const uint32_t offset = payload_offset + (Streams * slot + channel) * chunk_bytes;
                                const auto& input = ram.read_reg_array<int32_t, samples_per_chunk>(offset);
                                auto& output = snapshot.data(channel);
                                std::copy(input.begin(), input.end(), output.begin() + j * samples_per_chunk);
                            }
                        }
                    }
                    update_progress();
                    if (!acquisition_started.load()) return std::nullopt;
                    // Reserve the in-flight packet and the DDR writeback margin
                    // withheld by update_progress().
                    if (acquisition_window_is_intact(*window, write_count, ring_chunks - withheld_packets - 1)) {
                        if (complete) {
                            if (hop) {
                                for (uint32_t channel = 0; channel < Streams; ++channel) {
                                    const auto& output = snapshot.data(channel);
                                    cached[channel].assign(output.begin(), output.end());
                                }
                                cache_end = snapshot.end_chunk;
                                cache_generation = ring_generation;
                            }
                            return snapshot;
                        }
                        // An intact published window must have complete, full
                        // descriptors. Do not poll forever on malformed DMA data.
                        acquisition_started.store(false);
                        return std::nullopt;
                    }
                    // A copy raced a full ring lap; the next iteration selects
                    // a fresh intact window and records the skipped hops.
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return std::nullopt;
    }

    template<uint32_t data_size>
    std::optional<Snapshot<data_size>> read_xy(uint64_t consumed, const std::atomic<bool>& running,
                                             uint32_t hop = 0, bool initialized = false) {
        static_assert(Streams == 2);
        return read<data_size>(consumed, running, hop, initialized);
    }

  private:
    hw::Memory<mem::ram>& ram = hw::get_memory<mem::ram>();
    hw::Memory<mem::dma>& dma = hw::get_memory<mem::dma>();
    hw::Memory<mem::axi_hp0>& hp = hw::get_memory<mem::axi_hp0>();
    hw::Memory<mem::control>& ctl = hw::get_memory<mem::control>();
    AxisStreamPacketMux mux;
    mutable std::recursive_mutex transfer_mtx;
    mutable std::atomic<bool> acquisition_started{false};
    bool acquisition_requested = false;
    std::array<std::vector<int32_t>, Streams> cached;
    uint64_t cache_generation = 0, cache_end = 0;
    mutable uint64_t write_count = 0, packet_count = 0;
    uint64_t generation_base = 0, ring_generation = 0, ring_overruns = 0;
    mutable uint32_t last_sequence = 0;
    mutable std::chrono::steady_clock::time_point last_poll{};
    Frequency sampling{};
    mutable std::chrono::steady_clock::time_point last_progress{};

    void barrier() const {
#if defined(__arm__)
        asm volatile("dsb sy" ::: "memory");
#else
        std::atomic_thread_fence(std::memory_order_seq_cst);
#endif
    }
    uint32_t dma_descriptor_status(uint32_t slot) { return ram.read_reg(slot * descriptor_bytes + 28); }
    void update_progress() const {
        if (!acquisition_started.load()) return;
        if ((dma.read<0x34>() & 0x771u) != 0) { acquisition_started.store(false); return; }
        const uint32_t sequence = mux.get_packet_status() >> 8;
        const auto now = std::chrono::steady_clock::now();
        const double since_poll = std::chrono::duration<double>(now - last_poll).count();
        last_poll = now;
        if (since_poll * double(sampling.eval()) * Streams / samples_per_chunk >= double(1u << 24)) {
            acquisition_started.store(false);
            return;
        }
        if (sequence != last_sequence) last_progress = now;
        else if (std::chrono::duration<double>(now - last_progress).count() >
                 std::max(0.1, 3.0 * samples_per_chunk / double(sampling.eval()))) {
            acquisition_started.store(false);
            return;
        }
        packet_count += (sequence - last_sequence) & 0xffffffu;
        last_sequence = sequence;
        // TLAST precedes DDR writeback. Leave two packets for the payload
        // and SG writeback pipeline, matching the paired ring's margin; then
        // validate status and ring retention on copy.
        write_count = generation_base + (packet_count > withheld_packets ? (packet_count - withheld_packets) / Streams : 0);
    }
    bool reset_dma() {
        dma.write<0x30>(4);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(20);
        while (dma.read<0x30>() & 4u) {
            if (std::chrono::steady_clock::now() >= deadline) return false;
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
        return true;
    }
    void stop_ring() {
        ctl.write<reg::acquisition_run>(0); // Reset phase, filter and FIFO histories first.
        mux.stop();
        acquisition_started.store(false);
        (void)reset_dma();
    }
    bool start_ring() {
        ctl.write<reg::acquisition_run>(0);
        mux.stop();
        if (!reset_dma()) return false;
        generation_base = write_count;
        ++ring_generation;
        packet_count = 0;
        last_sequence = 0;
        last_progress = last_poll = std::chrono::steady_clock::now();
        constexpr auto address = hw::Memory<mem::ram>::phys_addr;
        for (uint32_t i = 0; i < descriptors; ++i) {
            const auto offset = i * descriptor_bytes;
            for (uint32_t j = 0; j < descriptor_bytes; j += 4) ram.write_reg(offset + j, 0u);
            ram.write_reg(offset, address + descriptor_bytes * ((i + 1) % descriptors));
            ram.write_reg(offset + 8, address + payload_offset + i * chunk_bytes);
            ram.write_reg(offset + 24, chunk_bytes);
        }
        barrier();
        const uint32_t first = Streams * (generation_base % ring_chunks);
        dma.write<0x38>(address + first * descriptor_bytes);
        dma.write<0x30>(0x11); // Run + cyclic BD enable.
        dma.write<0x40>(0x50); // Trigger; tail must be outside the cyclic chain.
        barrier();
        mux.start_continuous(samples_per_chunk, Streams == 2);
        ctl.write<reg::acquisition_run>(1); // ADC timeline begins after DMA is armed.
        return true;
    }
};
using CyclicPhaseDma = BasicCyclicPhaseDma<1>;
using PairedPhaseDma = BasicCyclicPhaseDma<2>;
#endif
