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

    void start_acquisition(Frequency fs_) {
        fs = fs_;
        bool expected = false;
        if (acquisition_started.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
            acq_thread = std::thread{&PhaseDma::acquisition_thread, this};
        }
    }

    template<uint32_t data_size>
    uint64_t get_offset_when_ready() {
        static constexpr uint32_t n_chunks_to_read = data_size / samples_per_chunk;

        while (write_idx.load(std::memory_order_acquire) < n_chunks_to_read) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        const uint64_t write_count_snapshot = write_idx.load(std::memory_order_acquire);
        const uint64_t first_chunk = write_count_snapshot - n_chunks_to_read;
        return first_chunk * chunk_bytes;
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

        return std::tuple{
            ram.read_reg_array<int32_t, data_size>(x_byte_offset + data_offset),
            ram.read_reg_array<int32_t, data_size>(y_byte_offset + data_offset)
        };
    }

  private:
    hw::Memory<mem::ram>& ram;
    DmaS2MM& dma;
    Frequency fs;
    std::atomic<uint32_t> write_idx = 0;

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

        const float chunk_duration = static_cast<float>(samples_per_chunk) / fs.eval();
        logf("PhaseDma::acquisition_thread: chunk_duration = {} ms\n", 1E3f * chunk_duration);

        axis_stream_mux.set_packet_length(samples_per_chunk);

        uint32_t idx = 0;
        while (acquisition_started.load(std::memory_order_acquire)) {
            uint32_t byte_offset = idx * chunk_bytes;

            axis_stream_mux.select_input(0);
            dma.start_transfer(dma_x_start_addr + byte_offset, chunk_bytes);
            axis_stream_mux.trigger();
            dma.wait_for_transfer(chunk_duration);

            axis_stream_mux.select_input(1);
            dma.start_transfer(dma_y_start_addr + byte_offset, chunk_bytes);
            axis_stream_mux.trigger();
            dma.wait_for_transfer(chunk_duration);

            write_idx.store(idx, std::memory_order_release);
            idx = (idx + 1) % n_chunks;
        }
    }
};

#endif // __PHASE_DMA_HPP__