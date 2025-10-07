/// Ltc2387 driver
///
/// (c) Koheron

#ifndef __ALPHA15_DRIVERS_LTC2387_HPP__
#define __ALPHA15_DRIVERS_LTC2387_HPP__

#include "server/context/context.hpp"

#include <array>

constexpr auto cal_coeffs_num = 4;

class Eeprom;

class Ltc2387
{
  public:
    Ltc2387(Context& ctx_);
    void init();

    // Data
    std::array<int32_t, 2> adc_raw_data(uint32_t n_avg);
    std::array<float, 2> adc_data_volts(uint32_t n_avg);

    // Clock Delay
    void set_clock_delay();
    void enable_adcs();
    void disable_adcs();
    void set_testpat();
    void clear_testpat();
    void clkout_dec();

    // Input Range
    enum input_range : uint32_t {
        RANGE_2V,
        RANGE_8V,
        input_range_num
    };

    void range_select(uint32_t channel, uint32_t range);
    uint32_t input_range(uint32_t channel);

    // Calibrations
    std::array<float, cal_coeffs_num> get_calibration(uint32_t channel);
    int32_t set_calibration(uint32_t channel, const std::array<float, cal_coeffs_num>& new_coeffs);
    float get_gain(uint32_t channel, uint32_t range) const;
    float get_offset(uint32_t channel, uint32_t range) const;

  private:
    Context& ctx;
    Memory<mem::control>& ctl;
    Memory<mem::status>& sts;
    Eeprom& eeprom;

    // Calibration array: [gain_2V, offset_2V, gain_8V, offset_8V]
    // gain in LSB / Volts
    // offset in LSB
    std::array<std::array<float, cal_coeffs_num>, 2> cal_coeffs;

    int32_t to_two_complement(int32_t data);
};

#endif // __ALPHA15_DRIVERS_LTC2387_HPP__
