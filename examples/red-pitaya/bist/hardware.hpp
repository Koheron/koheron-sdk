#ifndef KOHERON_BIST_HARDWARE_HPP
#define KOHERON_BIST_HARDWARE_HPP
#include "measurements.hpp"
#include "server/hardware/memory_manager.hpp"
#include "server/drivers/uio.hpp"
#include <atomic>
#include <thread>
namespace bist
{
inline void barrier()
{
#if defined(__arm__)
    asm volatile("dsb sy" ::: "memory");
#else
    std::atomic_thread_fence(std::memory_order_seq_cst);
#endif
}
// Identify the mapping that is actually in use, including SDK fallback.
inline std::string mapping_path(uintptr_t pointer)
{
    std::istringstream maps(read_file("/proc/self/maps"));
    std::string line;
    while (std::getline(maps, line))
    {
        std::istringstream fields(line);
        std::string range, permissions, offset, device, inode, path;
        fields >> range >> permissions >> offset >> device >> inode >> path;
        const auto dash = range.find('-');
        if (dash == std::string::npos)
            continue;
        uintptr_t low = 0, high = 0;
        std::istringstream(range.substr(0, dash)) >> std::hex >> low;
        std::istringstream(range.substr(dash + 1)) >> std::hex >> high;
        if (pointer >= low && pointer < high)
            return path + " " + permissions;
    }
    return "unmapped";
}
struct Transfer
{
    bool ok = false;
    double seconds = 0;
    uint64_t errors = 0;
    unsigned interrupts = 0;
    uint32_t tx_status = 0, rx_status = 0;
};
class Dma
{
  public:
    static constexpr uint32_t tx_data = 0x10000, rx_data = 0x400000;
    static constexpr uint32_t tx_bd = 0, rx_bd = 0x1000, chunk = 65536, bytes = 2 * 1024 * 1024;
    Dma(hw::Memory<mem::dma>& regs_, hw::Memory<mem::buffers>& ram_, hw::Memory<mem::control>& ctl_,
        std::atomic<bool>& cancelled_)
        : regs(regs_), ram(ram_), ctl(ctl_), cancelled(cancelled_)
    {
    }
    bool open() { return irq.open() >= 0; }
    bool reset()
    {
        ctl.write<reg::mode>(256);
        barrier();
        regs.write_reg(0, 4U);
        const auto deadline = Clock::now() + std::chrono::milliseconds(100);
        while (regs.read_reg(0) & 4)
        {
            if (cancelled || Clock::now() >= deadline)
                return false;
            std::this_thread::yield();
        }
        ctl.write<reg::mode>(0);
        barrier();
        return true;
    }
    void stop()
    {
        regs.write_reg(0, 0U);
        regs.write_reg(0x30, 0U);
        regs.write_reg(4, 0x7000U);
        regs.write_reg(0x34, 0x7000U);
        ctl.write<reg::mode>(256);
        barrier();
        // Complete shutdown before the overlay and mappings are released.
        regs.write_reg(0, 4U);
        const auto deadline = Clock::now() + std::chrono::milliseconds(100);
        while ((regs.read_reg(0) & 4) && Clock::now() < deadline)
            std::this_thread::yield();
    }
    void prepare(uint32_t size, bool tx, bool rx)
    {
        const unsigned count = (size + chunk - 1) / chunk;
        for (unsigned channel = 0; channel < 2; ++channel)
        {
            if ((channel == 0 && !tx) || (channel == 1 && !rx))
                continue;
            const uint32_t base = channel == 0 ? tx_bd : rx_bd;
            const uint32_t data = channel == 0 ? tx_data : rx_data;
            for (unsigned i = 0; i < count; ++i)
            {
                const uint32_t offset = base + 64 * i, length = std::min(chunk, size - i * chunk);
                for (unsigned j = 0; j < 16; ++j)
                    ram.write_reg(offset + 4 * j, 0U);
                ram.write_reg(offset, uint32_t(mem::buffers_addr) + base + 64 * ((i + 1) % count));
                ram.write_reg(offset + 8, uint32_t(mem::buffers_addr) + data + i * chunk);
                // One packet per TX BD; analog RX is a single capture packet.
                ram.write_reg(offset + 24, length | (channel == 0 ? 0x0c000000U : 0U));
            }
        }
        barrier();
    }
    Transfer transfer(uint32_t size, bool tx, bool rx, bool capture = false)
    {
        Transfer result;
        if (size == 0 || size > bytes || size % 4 != 0 || cancelled)
            return result;
        const unsigned count = (size + chunk - 1) / chunk;
        prepare(size, tx, rx);
        if (tx)
        {
            regs.write_reg(8, uint32_t(mem::buffers_addr) + tx_bd);
            regs.write_reg(0, 0x1001U | (count << 16));
        }
        if (rx)
        {
            regs.write_reg(0x38, uint32_t(mem::buffers_addr) + rx_bd);
            regs.write_reg(0x30, 0x1001U | (count << 16));
        }
        regs.write_reg(4, 0x7000U);
        regs.write_reg(0x34, 0x7000U);
        if (!irq.arm_irq())
            return result;
        const auto start = Clock::now(), deadline = start + std::chrono::seconds(2);
        if (rx)
            regs.write_reg(0x40, uint32_t(mem::buffers_addr) + rx_bd + 64 * (count - 1));
        if (tx)
            regs.write_reg(0x10, uint32_t(mem::buffers_addr) + tx_bd + 64 * (count - 1));
        if (capture)
            ctl.write<reg::capture>(ctl.read<reg::capture>() + 1);
        bool complete = false;
        while (!cancelled && Clock::now() < deadline)
        {
            const int observed = irq.wait_for_irq(std::chrono::milliseconds(20));
            if (observed < 0)
                break;
            if (observed > 0)
                ++result.interrupts;
            result.tx_status = regs.read_reg(4);
            result.rx_status = regs.read_reg(0x34);
            // Acknowledge both hardware causes before re-enabling the shared IRQ.
            regs.write_reg(4, 0x7000U);
            regs.write_reg(0x34, 0x7000U);
            barrier();
            const bool tx_done = !tx || (ram.read_reg(tx_bd + 64 * (count - 1) + 28) & 0x80000000U);
            const bool rx_done = !rx || (ram.read_reg(rx_bd + 64 * (count - 1) + 28) & 0x80000000U);
            if ((result.tx_status | result.rx_status) & 0x770U)
                break;
            if (tx_done && rx_done && observed > 0)
            {
                complete = true;
                break;
            }
            if (!irq.arm_irq())
                break;
        }
        result.seconds = bist::seconds(start);
        barrier();
        for (unsigned channel = 0; channel < 2; ++channel)
        {
            if ((channel == 0 && !tx) || (channel == 1 && !rx))
                continue;
            for (unsigned i = 0; i < count; ++i)
            {
                const uint32_t status = ram.read_reg((channel == 0 ? tx_bd : rx_bd) + 64 * i + 28);
                const uint32_t length = std::min(chunk, size - i * chunk);
                if (!(status & 0x80000000U) || (status & 0x70000000U) ||
                    (status & 0x7fffffU) != length)
                    ++result.errors;
            }
        }
        result.ok = complete && result.errors == 0 && result.interrupts > 0;
        regs.write_reg(0, 0U);
        regs.write_reg(0x30, 0U);
        return result;
    }
    void fill(uint32_t size, uint32_t seed)
    {
        ctl.write<reg::scratch>(seed);
        for (uint32_t i = 0; i < size / 4; ++i)
        {
            ram.write_reg(tx_data + 4 * i, pattern(i, seed));
            ram.write_reg(rx_data + 4 * i, ~pattern(i, seed));
        }
        barrier();
    }
    uint64_t verify(uint32_t size, uint32_t seed)
    {
        uint64_t errors = 0;
        for (uint32_t i = 0; i < size / 4; ++i)
            errors += ram.read_reg(rx_data + 4 * i) != pattern(i, seed);
        return errors;
    }

  private:
    hw::Memory<mem::dma>& regs;
    hw::Memory<mem::buffers>& ram;
    hw::Memory<mem::control>& ctl;
    std::atomic<bool>& cancelled;
    Uio<mem::dma> irq;
};
} // namespace bist
#endif
