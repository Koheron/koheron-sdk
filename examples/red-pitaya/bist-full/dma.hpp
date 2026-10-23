#pragma once
#include "../bist/hardware.hpp"
#include <numeric>

namespace bist_full
{
using bist::barrier;
using bist::Clock;
struct DmaResult
{
    bool completed = false, ok = false;
    uint32_t tx_status = 0, rx_status = 0, received = 0;
    uint64_t errors = 0, guards = 0;
    unsigned interrupts = 0;
    double seconds = 0;
};

// Descriptor and payload allocations use the first 8 MiB of the boot reservation.
class Dma
{
  public:
    static constexpr uint32_t tx_base = 0x10000, rx_base = 0x400000;
    static constexpr uint32_t maximum = 2 * 1024 * 1024, tx_bd = 0, rx_bd = 0x2000;
    hw::Memory<mem::dma> &regs = hw::get_memory<mem::dma>();
    hw::Memory<mem::buffers> &ram = hw::get_memory<mem::buffers>();
    hw::Memory<mem::control> &ctl = hw::get_memory<mem::control>();
    Uio<mem::dma> irq;
    std::atomic<bool> &cancel;
    explicit Dma(std::atomic<bool> &stop) : cancel(stop) {}
    bool open() { return irq.open() >= 0; }
    bool reset()
    {
        ctl.write<reg::mode>(256);
        barrier();
        regs.write_reg(0, 4U);
        const auto end = Clock::now() + std::chrono::milliseconds(100);
        while (regs.read_reg(0) & 4)
            if (Clock::now() >= end)
                return false;
        ctl.write<reg::fault>(0);
        ctl.write<reg::stalls>(0);
        ctl.write<reg::irq_period>(0);
        ctl.write<reg::mode>(0);
        barrier();
        return true;
    }
    void stop()
    {
        reset();
        ctl.write<reg::mode>(256);
        barrier();
    }
    static bool valid(const std::vector<uint32_t> &lengths, uint32_t offset)
    {
        if (lengths.empty() || lengths.size() > 64 || offset > 4096)
            return false;
        uint64_t total = 0;
        for (auto n : lengths)
        {
            if (!n || n > maximum)
                return false;
            total += n;
        }
        return total <= maximum;
    }
    void fill(uint32_t n, uint32_t seed, uint32_t tx_offset, uint32_t rx_offset)
    {
        for (uint32_t i = 0; i < n; ++i)
        {
            ram.write_reg<uint8_t>(tx_base + tx_offset + i,
                                   uint8_t(bist::pattern(i / 4, seed) >> ((i % 4) * 8)));
            ram.write_reg<uint8_t>(rx_base + rx_offset + i, 0xa5);
        }
        for (unsigned i = 1; i <= 64; ++i)
        {
            ram.write_reg<uint8_t>(tx_base + tx_offset - i, 0x5a);
            ram.write_reg<uint8_t>(tx_base + tx_offset + n + i - 1, 0x5a);
            ram.write_reg<uint8_t>(rx_base + rx_offset - i, 0x5a);
            ram.write_reg<uint8_t>(rx_base + rx_offset + n + i - 1, 0x5a);
        }
        barrier();
    }
    uint64_t verify(uint32_t n, uint32_t seed, uint32_t rx_offset)
    {
        uint64_t errors = 0;
        for (uint32_t i = 0; i < n; ++i)
            errors += ram.read_reg<uint8_t>(rx_base + rx_offset + i) !=
                      uint8_t(bist::pattern(i / 4, seed) >> ((i % 4) * 8));
        return errors;
    }
    uint64_t guards(uint32_t n, uint32_t tx_offset, uint32_t rx_offset)
    {
        uint64_t errors = 0;
        for (unsigned i = 1; i <= 64; ++i)
            for (auto base : {tx_base + tx_offset, rx_base + rx_offset})
            {
                errors += ram.read_reg<uint8_t>(base - i) != 0x5a;
                errors += ram.read_reg<uint8_t>(base + n + i - 1) != 0x5a;
            }
        return errors;
    }
    void descriptors(const std::vector<uint32_t> &lengths, uint32_t offset, bool tx)
    {
        const auto base = tx ? tx_bd : rx_bd;
        uint32_t data = (tx ? tx_base : rx_base) + offset;
        for (size_t i = 0; i < lengths.size(); ++i)
        {
            const uint32_t bd = base + 64 * i;
            for (unsigned j = 0; j < 16; ++j)
                ram.write_reg(bd + 4 * j, 0U);
            ram.write_reg(bd, uint32_t(mem::buffers_addr) + base + 64 * ((i + 1) % lengths.size()));
            ram.write_reg(bd + 8, uint32_t(mem::buffers_addr) + data);
            ram.write_reg(bd + 24, lengths[i] | (tx && i == 0 ? 0x08000000U : 0U) |
                                       (tx && i + 1 == lengths.size() ? 0x04000000U : 0U));
            data += lengths[i];
        }
    }
    DmaResult transfer(const std::vector<uint32_t> &lengths, uint32_t tx_offset = 0,
                       uint32_t rx_offset = 0, bool tx = true, bool rx = true, bool trigger = false,
                       unsigned timeout_ms = 250, unsigned descriptor_fault = 0,
                       bool mask_irq = false, const std::vector<uint32_t> &receive_lengths = {})
    {
        DmaResult r;
        const auto &incoming = receive_lengths.empty() ? lengths : receive_lengths;
        if (!valid(lengths, tx_offset) || !valid(incoming, rx_offset) || cancel ||
            std::accumulate(lengths.begin(), lengths.end(), uint64_t(0)) !=
                std::accumulate(incoming.begin(), incoming.end(), uint64_t(0)))
            return r;
        if (tx)
            descriptors(lengths, tx_offset, true);
        if (rx)
            descriptors(incoming, rx_offset, false);
        if (descriptor_fault == 1)
            ram.write_reg(tx_bd + 24, 0x0c000000U);
        if (descriptor_fault == 2)
            ram.write_reg(tx_bd + 28, 0x80000000U);
        barrier();
        regs.write_reg(4, 0x7000U);
        regs.write_reg(0x34, 0x7000U);
        uint32_t disabled = 0;
        if (::write(irq.fd(), &disabled, sizeof(disabled)) != sizeof(disabled))
            return r;
        while (irq.wait_for_irq(std::chrono::milliseconds(0)) > 0)
        {
        }
        if (tx)
        {
            regs.write_reg(8, uint32_t(mem::buffers_addr) + tx_bd);
            regs.write_reg(0, 0x1001U);
        }
        if (rx)
        {
            regs.write_reg(0x38, uint32_t(mem::buffers_addr) + rx_bd);
            regs.write_reg(0x30, 0x1001U);
        }
        uint32_t enabled = mask_irq ? 0 : 1;
        if (::write(irq.fd(), &enabled, sizeof(enabled)) != sizeof(enabled))
            return r;
        const auto begin = Clock::now(), end = begin + std::chrono::milliseconds(timeout_ms);
        const auto tx_tail = 64 * (lengths.size() - 1), rx_tail = 64 * (incoming.size() - 1);
        if (rx)
            regs.write_reg(0x40, uint32_t(mem::buffers_addr) + rx_bd + rx_tail);
        if (tx)
            regs.write_reg(0x10, uint32_t(mem::buffers_addr) + tx_bd + tx_tail);
        if (trigger)
            ctl.write<reg::capture>(ctl.read<reg::capture>() + 1);
        while (!cancel && Clock::now() < end)
        {
            const int event = irq.wait_for_irq(std::chrono::milliseconds(5));
            if (event < 0)
                break;
            if (event > 0)
                ++r.interrupts;
            r.tx_status = regs.read_reg(4);
            r.rx_status = regs.read_reg(0x34);
            regs.write_reg(4, 0x7000U);
            regs.write_reg(0x34, 0x7000U);
            barrier();
            if ((r.tx_status | r.rx_status) & 0x770U)
                break;
            const bool tx_done = !tx || (ram.read_reg(tx_bd + tx_tail + 28) & 0x80000000U);
            const bool rx_done = !rx || (ram.read_reg(rx_bd + rx_tail + 28) & 0x80000000U);
            if (tx_done && rx_done && event > 0)
            {
                r.completed = true;
                break;
            }
            if (!mask_irq && !irq.arm_irq())
                break;
        }
        r.seconds = bist::seconds(begin);
        barrier();
        for (bool transmit : {false, true})
        {
            if ((transmit && !tx) || (!transmit && !rx))
                continue;
            const auto &chain = transmit ? lengths : incoming;
            for (size_t i = 0; i < chain.size(); ++i)
            {
                const auto status = ram.read_reg((transmit ? tx_bd : rx_bd) + 64 * i + 28);
                r.errors += !(status & 0x80000000U) || (status & 0x70000000U) ||
                            (status & 0x7fffffU) != chain[i];
                if (!transmit)
                {
                    const uint32_t framing =
                        (i == 0 ? 0x08000000U : 0U) | (i + 1 == chain.size() ? 0x04000000U : 0U);
                    r.errors += (status & 0x0c000000U) != framing;
                    r.received += status & 0x7fffffU;
                }
            }
        }
        r.ok = r.completed && !r.errors && r.interrupts;
        regs.write_reg(0, 0U);
        regs.write_reg(0x30, 0U);
        return r;
    }
};
} // namespace bist_full
