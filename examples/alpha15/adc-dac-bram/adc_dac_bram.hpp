/// ADC DAC BRAM driver
///
/// (c) Koheron

#ifndef __DRIVERS_ADC_DAC_BRAM_HPP__
#define __DRIVERS_ADC_DAC_BRAM_HPP__

#include <context.hpp>
#include <boards/alpha15/drivers/clock-generator.hpp>

#include <array>
#include <thread>
#include <chrono>

constexpr uint32_t dac_size = mem::dac_range / sizeof(uint32_t);
constexpr uint32_t adc_size = mem::adc0_range / sizeof(uint32_t);

class AdcDacBram
{
  public:
    AdcDacBram(Context& ctx_)
    : ctx(ctx_)
    , ctl(ctx.mm.get<mem::control>())
    , sts(ctx.mm.get<mem::status>())
    , adc0_map(ctx.mm.get<mem::adc0>())
    , adc1_map(ctx.mm.get<mem::adc1>())
    , dac_map(ctx.mm.get<mem::dac>())
    , clockgen(ctx.get<ClockGenerator>())
    {
        const auto fs_adc = clockgen.get_adc_sampling_freq();
        t_acq[0] = int32_t(1E6 * adc_size / fs_adc[0]);
        t_acq[1] = int32_t(1E6 * adc_size / fs_adc[1]);
        ctx.log<INFO>("AdcDacBram: t_acq = {%li, %li} us\n", t_acq[0], t_acq[1]);
    }

    void trigger_acquisition() {
        ctl.set_bit<reg::trig, 0>();
        ctl.clear_bit<reg::trig, 0>();
    }

    uint32_t get_dac_size() {
        return dac_size;
    }

    uint32_t get_adc_size() {
        return adc_size;
    }

    void set_dac_data(const std::array<uint32_t, dac_size>& data) {
        dac_map.write_array(data);
    }

    auto get_adc(bool channel) {
        trigger_acquisition();
        std::this_thread::sleep_for(std::chrono::microseconds(t_acq[channel]));
        return channel ? adc1_map.read_array<uint32_t, adc_size>()
                       : adc0_map.read_array<uint32_t, adc_size>();
    }

    // IOs

    void set_b34_ios(uint32_t value) {
        ctl.write<reg::digital_outputs_b34>(value);
    }

 private:
    Context& ctx;
    hw::Memory<mem::control>& ctl;
    hw::Memory<mem::status>& sts;
    hw::Memory<mem::adc0>& adc0_map;
    hw::Memory<mem::adc1>& adc1_map;
    hw::Memory<mem::dac>& dac_map;
    ClockGenerator& clockgen;

    // Acquisition time for each ADC channel (microseconds)
    std::array<int32_t, 2> t_acq;
}; // class AdcDacBram

#endif // __DRIVERS_ADC_DAC_BRAM_HPP__