/// FIFO driver
///
/// (c) Koheron

// http://www.xilinx.com/support/documentation/ip_documentation/axi_fifo_mm_s/v4_1/pg080-axi-fifo-mm-s.pdf

#ifndef __SERVER_DRIVERS_FIFO_HPP__
#define __SERVER_DRIVERS_FIFO_HPP__

#include "server/runtime/services.hpp"
#include "server/runtime/syslog.hpp"
#include "server/hardware/memory_manager.hpp"
#include "server/drivers/uio.hpp"

#include <chrono>
#include <optional>
#include <string_view>

template <MemID fifo_mem>
class Fifo
{
  public:
    Fifo()
    : fifo(services::require<hw::MemoryManager>().get<fifo_mem>())
    {
        if constexpr (mem_dev == "/dev/uio") {
            uio.emplace();

            if (uio->open() >= 0) {
                irq_ready = true;
                enable_rx_irqs();
            } else {
                uio.reset();
            }
        }
    }

    uint32_t occupancy() {
        return read_reg<RDFO>();
    }

    void reset() {
        write_reg<SRR>(0xA5);
        write_reg<RDFR>(0xA5);
    }

    template<typename T = uint32_t>
    uint32_t read() {
        return read_reg<RDFD, T>();
    }

    uint32_t length() {
        return (read_reg<RLR>() & 0x3FFFFF) >> 2;
    }

    void wait_for_data(uint32_t n_pts, float fs_hz) {
        using namespace std::chrono;

        if (!irq_ready) { // Fallback: Sleep-based wait
            const auto fifo_duration = duration_cast<milliseconds>(duration<double>(n_pts / fs_hz));
            while (length() < n_pts) {
                std::this_thread::sleep_for(fifo_duration * 55 / 100); // ~0.55 * T
            }
            return;
        }

        // Reasonable timeout: a few FIFO durations; keep it bounded
        auto tmo = duration_cast<milliseconds>(duration<double>(n_pts / fs_hz));
        if (tmo < 1ms)  {
            tmo = 1ms;
        }

        if (tmo > 10s) {
            tmo = 10s;
        }

        // Loop until enough samples are present
        for (;;) {
            if (length() >= n_pts) {
                return;
            }

            // Re-arm IRQ *before* waiting to avoid dead time
            if (!uio->arm_irq()) { // Arming fails, degrade to sleep loop
                irq_ready = false;
                wait_for_data(n_pts, fs_hz);
                return;
            }

            const int rc = uio->wait_for_irq(tmo);

            if (rc < 0) { // Error: degrade to sleep loop
                irq_ready = false;
                wait_for_data(n_pts, fs_hz);
                return;
            }

            // Timeout: just loop; either more data will arrive or next arm+wait catches it
            if (rc == 0) {
                rt::print_fmt<WARNING>(
                    "FIFO [{}] Timed out. tmo={:%Q %q} [{} pts at {} Hz]\n",
                    mem_name, tmo, n_pts, fs_hz);
                continue;
            }

            // IRQ fired: acknowledge FIFO side by W1C the ISR
            const auto isr = read_reg<ISR>();
            write_reg<ISR>(isr); // W1C: write back set bits
        }
    }

    void probe() {
        rt::print_fmt<INFO>("FIFO [{}] probe: ISR={:#010x} IER={:#010x} RDFO={}\n",
            mem_name, read_reg<ISR>(), read_reg<IER>(), read_reg<RDFO>());
    }

  private:
    // AXI FIFO MM-S byte offsets (PG080)
    static constexpr uint32_t ISR  = 0x00; // Interrupt Status (RW1C)
    static constexpr uint32_t IER  = 0x04; // Interrupt Enable
    static constexpr uint32_t TDFR = 0x08; // TX FIFO Reset (W: 0xA5)
    static constexpr uint32_t RDFR = 0x18; // RX FIFO Reset (W: 0xA5)
    static constexpr uint32_t RDFO = 0x1C; // RX FIFO Occupancy (words)
    static constexpr uint32_t RDFD = 0x20; // RX Data (32-bit)
    static constexpr uint32_t RLR  = 0x24; // Receive length
    static constexpr uint32_t SRR  = 0x28; // Stream Reset (W: 0xA5)

    static constexpr std::string_view mem_name = mem::get_name(fifo_mem);
    static constexpr std::string_view mem_dev = mem::get_device_driver(fifo_mem);

    hw::Memory<fifo_mem>& fifo;

    std::optional<Uio<fifo_mem>> uio;
    bool irq_ready = false;

    void enable_rx_irqs() {
        // Clear any pending bits (W1C) and enable interrupts
        write_reg<ISR>(0xFFFFFFFFu);
        write_reg<IER>(0xFFFFFFFFu);
    }

    template<uint32_t reg, typename T = uint32_t>
    uint32_t read_reg() {
        return fifo.template read<reg, T>();
    }

    template<uint32_t reg, typename T = uint32_t>
    void write_reg(T value) {
        return fifo.template write<reg>(value);
    }
};

#endif // __SERVER_DRIVERS_FIFO_HPP__