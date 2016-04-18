/// Xilinx AXI FIFO reader
///
/// Starts a thread for continuous reading of
/// the AXI FIFO content. Read results are stored 
/// into a ring buffer of size N.
///
/// (c) Koheron

#ifndef __DRIVERS_CORE_FIFO_READER_HPP__
#define __DRIVERS_CORE_FIFO_READER_HPP__

#include <array>
#include <vector>
#include <algorithm>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>

#include <drivers/wr_register.hpp>

// http://www.xilinx.com/support/documentation/ip_documentation/axi_fifo_mm_s/v4_1/pg080-axi-fifo-mm-s.pdf
#define RDFR_OFF 0x18
#define RDFO_OFF 0x1C
#define RDFD_OFF 0x20
#define RLR_OFF  0x24

template<size_t N>
class FIFOReader
{
  public:
    FIFOReader();

    // Set FIFO virtual base address
    void set_address(uintptr_t fifo_addr_)
    {
        fifo_addr.store(fifo_addr_);
        Klib::WriteReg32(fifo_addr.load() + RDFR_OFF, 0xA5); // Reset FIFO
    }

    // Start the acquisition thread.
    // @acq_period_ Duration in microseconds between
    // two readings of the FIFO.
    void start_acquisition(uint32_t acq_period_);

    // Stop the acquisition thread.
    void stop_acquisition();

    // Acquisition status: true if the acquisition thread
    // is running false else.
    bool get_acquire_status() const {return is_acquiring.load();}

    // Number of u32 values in the last FIFO read
    uint32_t get_fifo_length() const {return fifo_length.load();}

    // Store the current of the ring buffer into the data buffer.
    // Return the size of the data buffer.
    uint32_t store_data();

    // Return a reference to the data buffer.
    // You must call store_data first to update the buffer.
    std::vector<uint32_t>& get_data() {return results_buffer;}

    // True if the ring buffer overflows
    bool overflow() {return acq_num.load() > N;}

  private:
    std::atomic<uintptr_t> fifo_addr;

    std::atomic<uint32_t> num_thread;

    std::atomic<uint32_t> index; // Current index of the ring_buffer
    std::atomic<uint32_t> acq_num; // Number of points acquire since the last call to get_data()
    std::atomic<uint32_t> fifo_length;
    std::array<uint32_t, N> ring_buffer;
    std::vector<uint32_t> results_buffer;
    std::mutex ring_buff_mtx; // Protect share access to the ring buffer

    std::atomic<bool> is_acquiring;
    void acquisition_thread_call(uint32_t acq_period);
};

template<size_t N>
FIFOReader<N>::FIFOReader()
{
    fifo_addr.store(0x0);
    is_acquiring.store(false);
    index.store(0);
    acq_num.store(0);
    fifo_length.store(0);
    results_buffer.reserve(N);

    num_thread.store(0);
}

template<size_t N>
void FIFOReader<N>::acquisition_thread_call(uint32_t acq_period)
{
    num_thread.store(num_thread.load() + 1);
    printf("num_thread = %u\n", num_thread.load());

    // uint32_t occupancy;

    is_acquiring.store(true);
    index.store(0);
    acq_num.store(0);

    while (is_acquiring.load()) {
        if (fifo_addr.load() == 0x0)
            goto wait;

        // occupancy = Klib::ReadReg32(fifo_addr.load() + RDFO_OFF);
        // // printf("occupancy = %u\n", occupancy);

        // if (occupancy > 0) {
            // The length is stored in the last 22 bits of the RLR register.
            // The length is given in bytes so we divide by 4 to get the number of u32.
            fifo_length.store((Klib::ReadReg32(fifo_addr.load() + RLR_OFF) & 0x3FFFFF) >> 2);
            // printf("fifo length = %u\n", fifo_length.load());

            if (fifo_length.load() > 0) {
                std::lock_guard<std::mutex> guard(ring_buff_mtx);
                for (uint32_t i=0; i<fifo_length.load(); i++) {
                    ring_buffer[index.load()] = Klib::ReadReg32(fifo_addr.load() + RDFD_OFF);
                    index.store((index.load() + 1) % N);
                }

                printf("%u\n", ring_buffer[index.load() - 1]);
            }

            acq_num.store(acq_num.load() + fifo_length.load());
        // }

wait:
        // TODO It would be nicer to catch the interrupt
        // from the FIFO in a blocking read for example. 
        std::this_thread::sleep_for(std::chrono::microseconds(acq_period));
    }

    num_thread.store(num_thread.load() - 1);
    printf("Exiting acquisition worker\n");
}

template<size_t N>
void FIFOReader<N>::start_acquisition(uint32_t acq_period)
{
    // If the is_acquiring flag is toogled during a the sleeping 
    // time of the thread, a new worker can be started while the
    // previous thread is still running since the is_acquiring flag
    // is true. So we count the number of acquisition threads to
    // be sure only one worker at the time is running.
    if (!is_acquiring.load() && num_thread.load() == 0) {
        std::thread acq_thread(&FIFOReader<N>::acquisition_thread_call, this, acq_period);
        acq_thread.detach();

        // Wait for the acquisition to start
        while (! is_acquiring.load()) {}
    }
}

template<size_t N>
void FIFOReader<N>::stop_acquisition()
{
    is_acquiring.store(false);
}

template<size_t N>
uint32_t FIFOReader<N>::store_data()
{
    uint32_t idx = index.load();

    if (!overflow()) {
        results_buffer.resize(idx);

        {
            std::lock_guard<std::mutex> guard(ring_buff_mtx);
            std::copy(ring_buffer.begin(), ring_buffer.begin() + idx, results_buffer.begin());
        }
    } else {
        results_buffer.resize(N);

        {
            std::lock_guard<std::mutex> guard(ring_buff_mtx);
            for (uint32_t i=0; i<N-idx; i++)
                results_buffer[i] = ring_buffer[idx + i];
            for (uint32_t i=N-idx-1; i<N; i++)
                results_buffer[i] = ring_buffer[i - N + idx + 1];
        }
    }

    index.store(0);
    acq_num.store(0);
    return results_buffer.size();
}

#endif // __DRIVERS_CORE_FIFO_READER_HPP__
