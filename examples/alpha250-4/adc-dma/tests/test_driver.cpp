#include "../adc_dma.hpp"
#include <cassert>
#include <iostream>
#include <limits>

template<int Descriptors> void complete(uint32_t samples) {
    auto& descriptors = hw::get_memory<Descriptors>();
    for (uint32_t i = 0; i < (samples + 65535) / 65536; ++i)
        descriptors.write_reg(64 * i + 28, 0x80000000U | (std::min(65536U, samples - i * 65536) * 4));
}
template<int Descriptors> void check_chain(uint32_t samples, uint32_t ram, uint32_t base) {
    auto& descriptors = hw::get_memory<Descriptors>();
    const auto count = (samples + 65535) / 65536;
    for (uint32_t i = 0; i < count; ++i) {
        assert(descriptors.read_reg(i * 64) == base + ((i + 1) % count) * 64);
        assert(descriptors.read_reg(i * 64 + 8) == ram + i * 262144);
        assert(descriptors.read_reg(i * 64 + 24) == std::min(65536U, samples - i * 65536) * 4);
        assert(descriptors.read_reg(i * 64 + 28) == 0);
    }
    assert(count * 64 <= 16384);
}
int main() {
    auto& slcr = hw::get_memory<mem::sclr>();
    slcr.write<0x910>(0b0101);
    AdcDma driver;
    assert(slcr.read<0x910>() == 0b1101);
    auto& ctl = hw::get_memory<mem::control>();
    auto& sts = hw::get_memory<mem::status>();
    auto& dma0 = hw::get_memory<mem::dma0>();
    auto& dma1 = hw::get_memory<mem::dma1>();
    auto& clock = rt::get_driver<ClockGenerator>();
    assert(ctl.read<reg::acq_reset>() == 1);
    assert(!driver.start());
    assert(!driver.arm());
    assert(!driver.trigger());
    assert(driver.get_adc_block(0, 0).empty());
    for (auto samples : {0U, 1U, 3U, 16777218U}) assert(!driver.configure(samples, false));
    clock.frequencies[1] = 250000000.0;
    assert(!driver.configure(8, false));
    clock.frequencies[1] = std::numeric_limits<double>::quiet_NaN();
    assert(!driver.configure(8, false));
    clock.frequencies[1] = 200000000.0;
    dma1.reset_stuck = true;
    assert(!driver.configure(8, false));
    dma1.reset_stuck = false;

    for (auto samples : {2U, 65536U, 65542U, 16000000U, 16777216U}) {
        assert(driver.configure(samples, true));
        check_chain<mem::ocm0>(samples, mem::ram0_addr, mem::ocm0_addr);
        check_chain<mem::ocm1>(samples, mem::ram1_addr, mem::ocm1_addr);
        assert(ctl.read<reg::sample_count>() == samples);
        assert(ctl.read<reg::test_pattern>() == 1);
        assert(driver.start());
        assert(!driver.start());
        const auto tail = ((samples + 65535) / 65536 - 1) * 64;
        assert(dma0.read<0x38>() == mem::ocm0_addr && dma1.read<0x38>() == mem::ocm1_addr);
        assert(dma0.read<0x40>() == mem::ocm0_addr + tail && dma1.read<0x40>() == mem::ocm1_addr + tail);
        sts.write<reg::capture_status>(2);
        sts.write<reg::captured_samples>(samples);
        complete<mem::ocm0>(samples);
        assert(driver.get_status() == 0); // The other DMA is still pending.
        assert(driver.get_adc_block(0, 0).empty());
        complete<mem::ocm1>(samples);
        assert(driver.get_status() == 1);
        if (samples <= 131072) {
            assert(driver.get_adc_block(0, 0).size() == std::min(samples, 65536U));
            assert(driver.get_adc_block(2, 0).empty());
            assert(driver.get_adc_block(0, (samples + 65535) / 65536).empty());
            if (samples == 65542) assert(driver.get_adc_block(1, 1).size() == 6);
        }
        driver.stop();
        assert(driver.get_adc_block(0, 0).empty());
        sts.write<reg::capture_status>(0);
    }

    // A finished tail alone cannot hide an earlier incomplete/error descriptor.
    assert(driver.configure(65542, false)); assert(driver.start());
    sts.write<reg::capture_status>(2); sts.write<reg::captured_samples>(65542);
    complete<mem::ocm0>(65542); complete<mem::ocm1>(65542);
    auto& descriptors = hw::get_memory<mem::ocm1>();
    const auto good = descriptors.read_reg(28);
    for (auto bad : {0U, good | 0x10000000U, good - 4}) {
        descriptors.write_reg(28, bad);
        assert(driver.get_status() == 4);
        assert(driver.get_adc_block(0, 0).empty());
    }
    descriptors.write_reg(28, good);
    sts.write<reg::captured_samples>(65540);
    assert(driver.get_status() == 4);
    sts.write<reg::captured_samples>(65542);
    dma0.write<0x34>(0x10); assert(driver.get_status() == 4); dma0.write<0x34>(0);
    dma1.write<0x34>(0x100); assert(driver.get_status() == 4); dma1.write<0x34>(0);
    sts.write<reg::capture_status>(36); assert(driver.get_status() == 0);
    sts.write<reg::capture_status>(6); assert(driver.get_status() == 2);
    sts.write<reg::capture_status>(10); assert(driver.get_status() == 3);
    sts.write<reg::capture_status>(16); assert(driver.get_status() == 5);
    sts.write<reg::capture_status>(0); sts.write<reg::pll_locked>(0); assert(driver.get_status() == 6);
    driver.stop();
    assert(!driver.configure(8, false)); // Unlock also prevents arming.
    sts.write<reg::pll_locked>(1);
    assert(driver.configure(2, true));
    assert(driver.arm());
    assert(!driver.arm());
    assert(ctl.read<reg::trigger>() == 0);
    assert(driver.get_status() == 0);
    assert(driver.trigger());
    assert(!driver.trigger());
    driver.stop();
    std::cout << "PASS: both SG chains, maximum/default/partial lengths, finite tails, reset timeout, completion and error checks\n";
}
