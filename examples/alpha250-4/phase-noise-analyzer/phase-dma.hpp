/// Continuous paired phase acquisition. Hardware alternates X/Y packets into
/// a cyclic SG DMA ring; Linux only copies completed, intact windows.
#ifndef __PHASE_DMA_HPP__
#define __PHASE_DMA_HPP__

#include <atomic>
#include <array>
#include <optional>
#include <mutex>
#include <algorithm>
#include <thread>
#include <chrono>
#include <scicpp/core.hpp>
#include "server/hardware/memory_manager.hpp"
#include "./axis-stream-packet-mux.hpp"
#include "./acquisition_window.hpp"

class PhaseDma {
    using Frequency = scicpp::units::frequency<float>;
  public:
    static constexpr uint32_t samples_per_chunk = 8192;
    static constexpr uint32_t ring_chunks = 512;
    static constexpr uint32_t descriptor_bytes = 64;
    static constexpr uint32_t descriptors = 2 * ring_chunks;
    static constexpr uint32_t payload_offset = descriptors * descriptor_bytes;
    static constexpr uint32_t chunk_bytes = samples_per_chunk * sizeof(int32_t);
    static_assert(payload_offset + descriptors * chunk_bytes <= hw::Memory<mem::ram>::size);

    PhaseDma() {
        ctl.write<reg::acquisition_run>(0);
        hp.set_bit<0x0, 0>();
        hp.set_bit<0x14, 0>();
    }
    ~PhaseDma() {
        std::lock_guard lock(transfer_mtx);
        stop_ring();
    }
    void set_fs(Frequency frequency) { sampling = frequency; }

    template<typename Apply>
    void configure_sampling(Frequency frequency, Apply&& apply) {
        std::lock_guard lock(transfer_mtx);
        const bool restart = acquisition_started.load();
        if (restart) { update_progress(); stop_ring(); }
        sampling = frequency;
        apply();
        if (restart) acquisition_started.store(start_ring());
    }
    void start_acquisition() {
        std::lock_guard lock(transfer_mtx);
        if (!acquisition_started.load()) acquisition_started.store(start_ring());
    }
    uint64_t completed_chunks() const {
        std::lock_guard lock(transfer_mtx);
        update_progress();
        return write_count;
    }

    template<uint32_t data_size> struct Snapshot {
        std::array<int32_t, data_size> x, y;
        uint64_t end_chunk;
        uint32_t precision_x = 0, precision_y = 0;
        bool overflow = false, mixed_precision = false, sample_gap = false;
        bool matches_precision(uint32_t requested) const {
            return !mixed_precision && precision_x == requested && precision_y == requested;
        }
    };

    template<uint32_t data_size>
    std::optional<Snapshot<data_size>> read_xy(uint64_t consumed, const std::atomic<bool>& keep_running) {
        static_assert(data_size > 0 && data_size % samples_per_chunk == 0);
        constexpr uint32_t chunks = data_size / samples_per_chunk;
        static_assert(chunks + 2 < ring_chunks);
        while (keep_running.load()) {
            {
                std::lock_guard lock(transfer_mtx);
                update_progress();
                // A configuration change temporarily stops the ring while
                // holding this mutex. Observe its final state only after the
                // restart, rather than mistaking that pause for a DMA failure.
                if (!acquisition_started.load()) return std::nullopt;
                auto window = acquisition_window(write_count, std::max(consumed, generation_base), chunks);
                if (window) {
                    Snapshot<data_size> snapshot;
                    snapshot.end_chunk = window->end_chunk;
                    barrier();
                    bool complete = true;
                    for (uint32_t j = 0; j < chunks; ++j) {
                        const auto chunk = window->first_chunk + j;
                        const uint32_t slot = chunk % ring_chunks;
                        const uint32_t metadata_slot = 2 * ((chunk - generation_base) % ring_chunks);
                        const auto x_status = mux.get_queued_status(metadata_slot);
                        const auto y_status = mux.get_queued_status(metadata_slot + 1);
                        for (uint32_t channel = 0; channel < 2; ++channel) {
                            const auto status = dma_descriptor_status(2 * slot + channel);
                            complete &= (status & 0xF3FFFFFFu) == (0x80000000u | chunk_bytes);
                        }
                        const uint32_t x_bits = x_status & 0xfu, y_bits = y_status & 0xfu;
                        if (j == 0) { snapshot.precision_x = x_bits; snapshot.precision_y = y_bits; }
                        snapshot.overflow |= ((x_status | y_status) & 0x10u) != 0;
                        snapshot.sample_gap |= ((x_status | y_status) & 0x40u) != 0;
                        snapshot.mixed_precision |= ((x_status | y_status) & 0x20u) != 0 ||
                            x_bits != snapshot.precision_x || y_bits != snapshot.precision_y;
                        const uint32_t offset = payload_offset + 2 * slot * chunk_bytes;
                        const auto& x = ram.read_reg_array<int32_t, samples_per_chunk>(offset);
                        const auto& y = ram.read_reg_array<int32_t, samples_per_chunk>(offset + chunk_bytes);
                        std::copy(x.begin(), x.end(), snapshot.x.begin() + j * samples_per_chunk);
                        std::copy(y.begin(), y.end(), snapshot.y.begin() + j * samples_per_chunk);
                    }
                    update_progress();
                    // Reserve the unpublished/in-flight pair as well as the
                    // completed pair withheld by update_progress().
                    if (complete && acquisition_window_is_intact(*window, write_count, ring_chunks - 2)) return snapshot;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return std::nullopt;
    }

  private:
    hw::Memory<mem::ram>& ram = hw::get_memory<mem::ram>();
    hw::Memory<mem::dma>& dma = hw::get_memory<mem::dma>();
    hw::Memory<mem::axi_hp0>& hp = hw::get_memory<mem::axi_hp0>();
    hw::Memory<mem::control>& ctl = hw::get_memory<mem::control>();
    AxisStreamPacketMux mux;
    mutable std::recursive_mutex transfer_mtx;
    mutable std::atomic<bool> acquisition_started{false};
    mutable uint64_t write_count = 0, packet_count = 0;
    uint64_t generation_base = 0;
    mutable uint32_t last_sequence = 0;
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
        if (sequence != last_sequence) last_progress = now;
        else if (std::chrono::duration<double>(now - last_progress).count() >
                 std::max(0.1, 3.0 * samples_per_chunk / double(sampling.eval()))) {
            acquisition_started.store(false);
            return;
        }
        packet_count += (sequence - last_sequence) & 0xffffffu;
        last_sequence = sequence;
        // Packet TLAST can reach the mux before DDR writes complete. Withhold
        // one pair: the next pair's SG descriptors cannot run before the prior
        // payload and descriptor writeback finish. Validate writeback on copy.
        write_count = generation_base + (packet_count >= 2 ? packet_count / 2 - 1 : 0);
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
        ctl.write<reg::acquisition_run>(0); // Reset both sample histories first.
        mux.stop();
        acquisition_started.store(false);
        (void)reset_dma();
    }
    bool start_ring() {
        ctl.write<reg::acquisition_run>(0);
        mux.stop();
        if (!reset_dma()) return false;
        generation_base = write_count;
        packet_count = 0;
        last_sequence = 0;
        last_progress = std::chrono::steady_clock::now();
        constexpr auto address = hw::Memory<mem::ram>::phys_addr;
        for (uint32_t i = 0; i < descriptors; ++i) {
            const auto offset = i * descriptor_bytes;
            for (uint32_t j = 0; j < descriptor_bytes; j += 4) ram.write_reg(offset + j, 0u);
            ram.write_reg(offset, address + descriptor_bytes * ((i + 1) % descriptors));
            ram.write_reg(offset + 8, address + payload_offset + i * chunk_bytes);
            ram.write_reg(offset + 24, chunk_bytes);
        }
        barrier();
        const uint32_t first = 2 * (generation_base % ring_chunks);
        dma.write<0x38>(address + first * descriptor_bytes);
        dma.write<0x30>(0x11); // Run + cyclic BD enable.
        dma.write<0x40>(0x50); // Trigger; tail must be outside the cyclic chain.
        barrier();
        mux.start_continuous(samples_per_chunk);
        ctl.write<reg::acquisition_run>(1); // ADC timeline begins after DMA is armed.
        return true;
    }
};
#endif
