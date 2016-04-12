/// Xilinx AXI FIFO reader
///
/// (c) Koheron

#ifndef __DRIVERS_CORE_FIFO_READER_HPP__
#define __DRIVERS_CORE_FIFO_READER_HPP__

#include <array>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>

#include <drivers/wr_register.hpp>

// http://www.xilinx.com/support/documentation/ip_documentation/axi_fifo_mm_s/v4_1/pg080-axi-fifo-mm-s.pdf
#define PEAK_RDFR_OFF 0x18
#define PEAK_RDFO_OFF 0x1C
#define PEAK_RDFD_OFF 0x20
#define PEAK_RLR_OFF  0x24

template<size_t N>
class FIFOReader
{
  public:
    FIFOReader();

    void set_address(uintptr_t fifo_addr_);
    void start_acquisition(uint32_t acq_period_);
    void stop_acquisition();
    uint32_t get_acq_count() const {return acq_num;}
    bool get_acquire_status() const {return acquire.load();}
    uint32_t get_fifo_length() const {return fifo_length.load();}

    // Store the current of the ring buffer into the data buffer.
    // Return the size of the data buffer.
    uint32_t store_data();

    // Return a reference to the data buffer.
    std::vector<uint32_t>& get_data();

  private:
    uintptr_t fifo_addr;

    std::atomic<uint32_t> index; // Current index of the ring_buffer
    std::atomic<uint32_t> acq_cnt;
    uint32_t acq_num;
    std::atomic<uint32_t> fifo_length;
    std::array<uint32_t, N> ring_buffer;
    std::vector<uint32_t> results_buffer;
    std::mutex buff_access_mtx;
    std::thread acq_thread;

    std::atomic<bool> acquire;
    void acquisition_thread_call(uint32_t acq_period);
};

// cpp file

template<size_t N>
FIFOReader<N>::FIFOReader()
: fifo_addr(0)
{
    acquire.store(false);
    index.store(0);
    acq_cnt.store(0);
    fifo_length.store(0);
    results_buffer.reserve(N);
}

template<size_t N>
void FIFOReader<N>::set_address(uintptr_t fifo_addr_)
{
    fifo_addr = fifo_addr_;
}

template<size_t N>
void FIFOReader<N>::acquisition_thread_call(uint32_t acq_period)
{
    // Reset FIFO
    Klib::WriteReg32(fifo_addr + PEAK_RDFR_OFF, 0x000000A5);

    acquire.store(true);

    while (acquire.load()) {
        // The length is stored in the last 22 bits of the RLR register.
        // The length is given in bytes so we divide by 4 to get the number of u32.
        fifo_length.store((Klib::ReadReg32(fifo_addr + PEAK_RLR_OFF) & 0x3FFFFF) >> 2);

        // buff_access_mtx.lock();
        for (uint32_t i=0; i<fifo_length.load(); i++) {
            acq_cnt.store(acq_cnt.load() + 1);
            ring_buffer[index.load()] = Klib::ReadReg32(fifo_addr + PEAK_RDFD_OFF);
            index.store((index.load() + 1) % N);
        }
        // buff_access_mtx.unlock();

        std::this_thread::sleep_for(std::chrono::milliseconds(acq_period));
    }
}

template<size_t N>
void FIFOReader<N>::start_acquisition(uint32_t acq_period)
{
    acq_thread = std::thread{&FIFOReader<N>::acquisition_thread_call, this, acq_period};
    acq_thread.detach();
}

template<size_t N>
void FIFOReader<N>::stop_acquisition()
{
    acquire.store(false);
    index.store(0);
    acq_cnt.store(0);
}

template<size_t N>
uint32_t FIFOReader<N>::store_data()
{
    uint32_t idx = index.load();

    if (idx < N) { // Less than one turn of the ring buffer
        results_buffer.resize(idx);
        for (uint32_t i=0; i<idx; i++)
            results_buffer[i] = ring_buffer[i];
    } else {
        results_buffer.resize(N);

        for (uint32_t i=0; i<N-idx; i++)
            results_buffer[i] = ring_buffer[idx + i];
        for (uint32_t i=N-idx-1; i<N; i++)
            results_buffer[i] = ring_buffer[i - N + idx + 1];
    }

    acq_num = acq_cnt.load();
    index.store(0);
    acq_cnt.store(0);

    return results_buffer.size();
}

template<size_t N>
std::vector<uint32_t>& FIFOReader<N>::get_data()
{
    return results_buffer;
}

#endif // __DRIVERS_CORE_FIFO_READER_HPP__