/// PhaseDma driver
///
/// (c) Koheron

#ifndef __PHASE_DMA_HPP__
#define __PHASE_DMA_HPP__

#include <atomic>
#include <thread>
#include <tuple>
#include <scicpp/core.hpp>

#include "server/runtime/syslog.hpp"
#include "server/hardware/memory_manager.hpp"
#include "server/drivers/dma-s2mm.hpp"

#include "./axis-stream-packet-mux.hpp"

class PhaseDma
{
    using Time = scicpp::units::time<float>;
    using Frequency = scicpp::units::frequency<float>;

  public:
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
        fs = fs_;
        chunk_duration.store(static_cast<float>(samples_per_chunk) / fs, std::memory_order_release);
        logf("PhaseDma::set_fs: chunk_duration = {} ms\n", 1E3f * chunk_duration.load(std::memory_order_relaxed).eval());
    }

    void start_acquisition() {
        bool expected = false;
        if (acquisition_started.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
            acq_thread = std::thread{&PhaseDma::acquisition_thread, this};
        }
    }

    template<uint32_t data_size>
    uint64_t get_offset_when_ready() {
        static_assert(data_size % samples_per_chunk == 0);
        static_assert(data_size <= buffer_size / bytes_per_sample);

        static constexpr uint32_t n_chunks_to_read = data_size / samples_per_chunk;

        while (write_count.load(std::memory_order_acquire) < n_chunks_to_read) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        const uint64_t count = write_count.load(std::memory_order_acquire);
        uint64_t first_chunk = count - n_chunks_to_read;
        uint64_t ring_chunk = first_chunk % n_chunks;

        // `read_reg_array` requires a contiguous memory region.
        // If the selected chunk window crosses the ring-buffer boundary, move to the
        // previous contiguous window within the same ring lap to avoid out-of-range access.
        if (ring_chunk + n_chunks_to_read > n_chunks) {
            if (first_chunk >= ring_chunk) {
                first_chunk -= ring_chunk;
            } else {
                first_chunk = 0;
            }
            ring_chunk = first_chunk % n_chunks;
        }

        return ring_chunk * chunk_bytes;
    }

    template<uint32_t data_size>
    auto& data_x() {
        const uint64_t data_offset = get_offset_when_ready<data_size>();
        return ram.read_reg_array<int32_t, data_size>(x_byte_offset + data_offset);
    }

    template<uint32_t data_size>
    auto& data_y() {
        const uint64_t data_offset = get_offset_when_ready<data_size>();
        return ram.read_reg_array<int32_t, data_size>(y_byte_offset + data_offset);
    }

    template<uint32_t data_size>
    auto data_xy() {
        const uint64_t data_offset = get_offset_when_ready<data_size>();

        return std::tie(
            ram.read_reg_array<int32_t, data_size>(x_byte_offset + data_offset),
            ram.read_reg_array<int32_t, data_size>(y_byte_offset + data_offset)
        );
    }

  private:
    hw::Memory<mem::ram>& ram;
    DmaS2MM& dma;
    Frequency fs;
    std::atomic<Time> chunk_duration{Time(0.0f)};
    std::atomic<uint64_t> write_count{0};

    // RAM ring buffers
    // Acquisition loops continuously fills 2 circular buffers in RAM (One for X data, the other for Y data)
    // RAM size is 128M so 2 buffers of 64 * 1024 * 1024 bytes.
    static constexpr uint32_t samples_per_chunk = 8192;
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
            const uint32_t idx = count % n_chunks;
            const uint32_t byte_offset = idx * chunk_bytes;

            axis_stream_mux.select_input(0);
            dma.start_transfer(dma_x_start_addr + byte_offset, chunk_bytes);
            axis_stream_mux.trigger();
            dma.wait_for_transfer(chunk_duration.load(std::memory_order_acquire));

            axis_stream_mux.select_input(1);
            dma.start_transfer(dma_y_start_addr + byte_offset, chunk_bytes);
            axis_stream_mux.trigger();
            dma.wait_for_transfer(chunk_duration.load(std::memory_order_acquire));

            write_count.store(count + 1, std::memory_order_release);
            ++count;
        }
    }
};

#endif // __PHASE_DMA_HPP__
