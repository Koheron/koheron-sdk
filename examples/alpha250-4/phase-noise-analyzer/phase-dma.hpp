/// PhaseDma driver
///
/// (c) Koheron

#ifndef __PHASE_DMA_HPP__
#define __PHASE_DMA_HPP__

#include <atomic>
#include <array>
#include <optional>
#include <mutex>
#include <algorithm>
#include <thread>
#include <tuple>
#include <scicpp/core.hpp>

#include "server/runtime/syslog.hpp"
#include "server/hardware/memory_manager.hpp"
#include "server/drivers/dma-s2mm.hpp"

#include "./axis-stream-packet-mux.hpp"
#include "./acquisition_window.hpp"

class PhaseDma
{
    using Time = scicpp::units::time<float>;
    using Frequency = scicpp::units::frequency<float>;

  public:
    static constexpr uint32_t samples_per_chunk = 8192;

    PhaseDma()
    : ram(hw::get_memory<mem::ram>())
    , dma(rt::get_driver<DmaS2MM>())
    {}

    ~PhaseDma() {
        acquisition_started.store(false, std::memory_order_release);
        if (acq_thread.joinable()) {
            acq_thread.join();
        }
    }

    void set_fs(Frequency fs_) {
        // Share the polling budget between X and Y to sustain the pair rate.
        chunk_duration.store(0.5f * static_cast<float>(samples_per_chunk) / fs_, std::memory_order_release);
        logf("PhaseDma::set_fs: chunk_duration = {} ms\n", 1E3f * chunk_duration.load(std::memory_order_relaxed).eval());
    }

    template<typename Apply>
    void configure_sampling(Frequency sampling, Apply&& apply) {
        // Keep a live rate change between complete X/Y transfers so a transfer
        // cannot use the old timeout while the FPGA is producing at the new rate.
        std::lock_guard lock(transfer_mtx);
        set_fs(sampling);
        apply();
    }

    void start_acquisition() {
        bool expected = false;
        if (acquisition_started.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
            acq_thread = std::thread{&PhaseDma::acquisition_thread, this};
        }
    }

    uint64_t completed_chunks() const {
        return write_count.load(std::memory_order_acquire);
    }

    template<uint32_t data_size>
    struct Snapshot {
        std::array<int32_t, data_size> x;
        std::array<int32_t, data_size> y;
        uint64_t end_chunk;
    };

    template<uint32_t data_size>
    std::optional<Snapshot<data_size>> read_xy(
        uint64_t consumed, const std::atomic<bool>& keep_running) {
        static_assert(data_size > 0 && data_size % samples_per_chunk == 0);
        static_assert(data_size < buffer_size / bytes_per_sample);
        constexpr uint32_t chunks = data_size / samples_per_chunk;

        while (keep_running.load(std::memory_order_acquire) &&
               acquisition_started.load(std::memory_order_acquire)) {
            auto window = acquisition_window(completed_chunks(), consumed, chunks);
            if (!window) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            Snapshot<data_size> snapshot;
            snapshot.end_chunk = window->end_chunk;
            for (uint32_t j = 0; j < chunks; ++j) {
                const uint32_t offset = ((window->first_chunk + j) % n_chunks) * chunk_bytes;
                const auto& x = ram.read_reg_array<int32_t, samples_per_chunk>(x_byte_offset + offset);
                const auto& y = ram.read_reg_array<int32_t, samples_per_chunk>(y_byte_offset + offset);
                std::copy(x.begin(), x.end(), snapshot.x.begin() + j * samples_per_chunk);
                std::copy(y.begin(), y.end(), snapshot.y.begin() + j * samples_per_chunk);
            }

            if (acquisition_window_is_intact(*window, completed_chunks(), n_chunks)) {
                return snapshot;
            }
            // The producer overtook the copy. Retry using the newest complete window.
        }
        return std::nullopt;
    }

  private:
    hw::Memory<mem::ram>& ram;
    DmaS2MM& dma;
    std::mutex transfer_mtx;
    std::atomic<Time> chunk_duration{Time(0.0f)};
    std::atomic<uint64_t> write_count{0};

    // RAM ring buffers
    // Acquisition loops continuously fills 2 circular buffers in RAM (One for X data, the other for Y data)
    // RAM size is 128M so 2 buffers of 64 * 1024 * 1024 bytes.
    static constexpr uint32_t bytes_per_sample = sizeof(int32_t);
    static constexpr uint32_t chunk_bytes = samples_per_chunk * bytes_per_sample;
    static constexpr uint32_t buffer_size = 64 * 1024 * 1024;
    static constexpr uint32_t n_chunks = buffer_size / chunk_bytes;
    static constexpr uint32_t x_byte_offset = 0;
    static constexpr uint32_t y_byte_offset = buffer_size;

    AxisStreamPacketMux axis_stream_mux;

    // Data acquisition thread
    std::thread acq_thread;
    std::atomic<bool> acquisition_started{false};

    void acquisition_thread() {
        constexpr auto dma_phys_addr = hw::Memory<mem::ram>::phys_addr;
        constexpr auto dma_x_start_addr = dma_phys_addr + x_byte_offset;
        constexpr auto dma_y_start_addr = dma_phys_addr + y_byte_offset;

        axis_stream_mux.set_packet_length(samples_per_chunk);

        uint64_t count = 0;
        while (acquisition_started.load(std::memory_order_acquire)) {
            std::lock_guard lock(transfer_mtx);
            const uint32_t idx = count % n_chunks;
            const uint32_t byte_offset = idx * chunk_bytes;

            axis_stream_mux.select_input(0);
            dma.start_transfer(dma_x_start_addr + byte_offset, chunk_bytes);
            axis_stream_mux.trigger();
            if (!dma.wait_for_transfer(chunk_duration.load(std::memory_order_acquire))) {
                acquisition_started.store(false, std::memory_order_release);
                return;
            }

            axis_stream_mux.select_input(1);
            dma.start_transfer(dma_y_start_addr + byte_offset, chunk_bytes);
            axis_stream_mux.trigger();
            if (!dma.wait_for_transfer(chunk_duration.load(std::memory_order_acquire))) {
                acquisition_started.store(false, std::memory_order_release);
                return;
            }

            write_count.store(count + 1, std::memory_order_release);
            ++count;
        }
    }
};

#endif // __PHASE_DMA_HPP__
