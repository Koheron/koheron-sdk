/// Ltc2387 driver
///
/// (c) Koheron

#ifndef __ALPHA15_DRIVERS_LTC2387_HPP__
#define __ALPHA15_DRIVERS_LTC2387_HPP__

#include <context.hpp>

#include "eeprom.hpp"
#include "clock-generator.hpp"

#include <array>
#include <cmath>
#include <chrono>
#include <thread>
#include <limits>

constexpr auto cal_coeffs_num = 4;

class Ltc2387
{
  public:
    Ltc2387(Context& ctx_)
    : ctx(ctx_)
    , ctl(ctx.mm.get<mem::control>())
    , sts(ctx.mm.get<mem::status>())
    , eeprom(ctx.get<Eeprom>())
    , clkgen(ctx.get<ClockGenerator>())
    {}

    void init() {
        // Load calibrations
        eeprom.read<eeprom_map::rf_adc_ch0_calib::offset>(cal_coeffs[0]);
        eeprom.read<eeprom_map::rf_adc_ch1_calib::offset>(cal_coeffs[1]);

        // Clock generator must be initialized before enabling ADC
        enable_adcs();
        range_select(0, 0);
        range_select(1, 0);
        set_clock_delay();
    }

    void set_clock_delay() {
        using namespace std::literals;

        ctx.log<INFO>("Ltc2387: Setting ADC clock delay ...\n");

        // Two lane mode test pattern
        constexpr int32_t testpat = 0b110011000011111100;

        set_testpat();
        std::this_thread::sleep_for(50us);

        const auto t1 = std::chrono::high_resolution_clock::now();

        // Skip first readings if testpattern is already good
        int32_t data0 = testpat;
        int32_t data1 = testpat;

        while (data0 == testpat && data1 == testpat) {
            clkgen.phase_shift(1);
            data0 = sts.read<reg::adc0, int32_t>();
            data1 = sts.read<reg::adc1, int32_t>();
        }

        ctx.log<INFO>("Ltc2387: testpat = 0x%05x, data0 = 0x%05x, data1 = 0x%05x\n",
                      testpat, data0, data1);

        // Increase phase-shift until we recover the test pattern
        while (data0 != testpat || data1 != testpat) {
            clkgen.phase_shift(1);
            data0 = sts.read<reg::adc0, int32_t>();
            data1 = sts.read<reg::adc1, int32_t>();
        }

        ctx.log<INFO>("Ltc2387: testpat = 0x%05x, data0 = 0x%05x, data1 = 0x%05x\n",
                      testpat, data0, data1);

        int32_t cnt_good_shifts = 0; // Count the size of the good pattern window

        while (data0 == testpat && data1 == testpat) {
            ++cnt_good_shifts;
            clkgen.phase_shift(1);
            data0 = sts.read<reg::adc0, int32_t>();
            data1 = sts.read<reg::adc1, int32_t>();
        }

        ctx.log<INFO>("Ltc2387: cnt_good_shifts = %li\n", cnt_good_shifts);
        ctx.log<INFO>("Ltc2387: testpat = 0x%05x, data0 = 0x%05x, data1 = 0x%05x\n",
                      testpat, data0, data1);

        // Increase phase-shift until we recover the test pattern
        while (data0 != testpat || data1 != testpat) {
            clkgen.phase_shift(1);
            data0 = sts.read<reg::adc0, int32_t>();
            data1 = sts.read<reg::adc1, int32_t>();
        }

        ctx.log<INFO>("Ltc2387: testpat = 0x%05x, data0 = 0x%05x, data1 = 0x%05x\n",
                      testpat, data0, data1);

        // Add half of the good pattern window
        clkgen.phase_shift(cnt_good_shifts / 2);

        data0 = sts.read<reg::adc0, int32_t>();
        data1 = sts.read<reg::adc1, int32_t>();

        ctx.log<INFO>("Ltc2387: testpat = 0x%05x, data0 = 0x%05x, data1 = 0x%05x\n",
                      testpat, data0, data1);

        if (data0 != testpat || data1 != testpat) {
            ctx.log<ERROR>("Ltc2387: Failed to set ADC clock delay\n");
        }

        const auto t2 = std::chrono::high_resolution_clock::now();
        const auto duration = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
        ctx.log<INFO>("Ltc2387: Delay clock adjustment duration %lu us\n", duration);

        clear_testpat();
    }

    // reg::rf_adc_ctl
    // BIT  0      : RANGE
    // BIT  1      : TESTPAT
    // BIT  2      : ENABLE
    // BIT  3      : CLKOUT_DEC (Only ADC0 is used)

    void enable_adcs() {
        ctl.set_bit<reg::rf_adc_ctl0, 2>();
        ctl.set_bit<reg::rf_adc_ctl1, 2>();
    }

    void disable_adcs() {
        ctl.clear_bit<reg::rf_adc_ctl0, 2>();
        ctl.clear_bit<reg::rf_adc_ctl1, 2>();
    }

    void set_testpat() {
        ctl.set_bit<reg::rf_adc_ctl0, 1>();
        ctl.set_bit<reg::rf_adc_ctl1, 1>();
    }

    void clear_testpat() {
        ctl.clear_bit<reg::rf_adc_ctl0, 1>();
        ctl.clear_bit<reg::rf_adc_ctl1, 1>();
    }

    void clkout_dec() {
        ctl.set_bit<reg::rf_adc_ctl0, 3>();
        ctl.clear_bit<reg::rf_adc_ctl0, 3>();
    }

    enum input_range : uint32_t {
        RANGE_2V,
        RANGE_8V,
        input_range_num
    };

    void range_select(uint32_t channel, uint32_t range) {
        // range = 0: Input range 2 Vpp
        // range = 1: Input range 8 Vpp

        if (range >= input_range_num) {
            ctx.log<ERROR>("Ltc2387::range_select: Invalid range\n");
            return;
        }

        if (channel >= 2) {
            ctx.log<ERROR>("Ltc2387::range_select: Invalid channel\n");
            return;
        }

        channel ? ctl.write_bit<reg::rf_adc_ctl1, 0>(range)
                : ctl.write_bit<reg::rf_adc_ctl0, 0>(range);

        ctx.log<INFO>("Ltc2387: Channel %u set to range %s\n",
                      channel, range ? "8 Vpp": "2 Vpp");
    }

    uint32_t input_range(uint32_t channel) {
        return channel ? ctl.read_bit<reg::rf_adc_ctl1, 0>()
                       : ctl.read_bit<reg::rf_adc_ctl0, 0>();
    }

    // Data input delay

    template<uint32_t regid, uint32_t lsb>
    void set_delay_tap(uint32_t tap) {
        if (tap > 31U) {
            ctx.log<WARNING>("Ltc2387: Tap too large [%u > 31]\n", tap);
            tap = 31U;
        }

        constexpr auto mask = ~((0b11111) << lsb);
        ctl.write<regid>((ctl.read<regid, uint32_t>() & mask) | (tap << lsb));

        // Reset delay
        ctl.set_bit<regid, 21>();
        ctl.clear_bit<regid, 21>();
    }

    void dco_delay_tap(bool channel, uint32_t tap) {
        channel ? set_delay_tap<reg::rf_adc_ctl1, 4>(tap)
                : set_delay_tap<reg::rf_adc_ctl0, 4>(tap);
    }

    void da_delay_tap(bool channel, uint32_t tap) {
        channel ? set_delay_tap<reg::rf_adc_ctl1, 9>(tap)
                : set_delay_tap<reg::rf_adc_ctl0, 9>(tap);
    }

    void db_delay_tap(bool channel, uint32_t tap) {
        channel ? set_delay_tap<reg::rf_adc_ctl1, 15>(tap)
                : set_delay_tap<reg::rf_adc_ctl0, 15>(tap);
    }

    // Set the same delay on all inputs
    void set_input_delay(uint32_t tap) {
        dco_delay_tap(0, tap);
        dco_delay_tap(1, tap);
        da_delay_tap(0, tap);
        da_delay_tap(1, tap);
        db_delay_tap(0, tap);
        db_delay_tap(1, tap);
    }

    // Data

    auto adc_raw_data(uint32_t n_avg) {
        if (n_avg <= 1) {
            return std::array{sts.read<reg::adc0, int32_t>(),
                              sts.read<reg::adc1, int32_t>()};
        } else  {
            int64_t adc0 = 0;
            int64_t adc1 = 0;

            for (size_t i=0; i<n_avg; ++i) {
                adc0 += sts.read<reg::adc0, int32_t>();
                adc1 += sts.read<reg::adc1, int32_t>();
            }

            return std::array{
                int32_t(std::round(adc0 / double(n_avg))),
                int32_t(std::round(adc1 / double(n_avg)))
            };
        }
    }

    auto adc_data_volts(uint32_t n_avg) {
        const auto [data_raw0, data_raw1] = adc_raw_data(n_avg);
        const auto val0 = float(to_two_complement(data_raw0));
        const auto val1 = float(to_two_complement(data_raw1));

        const auto range0 = input_range(0);
        const auto range1 = input_range(1);
        const auto gain0 = get_gain(0, range0);     // LSB/V
        const auto gain1 = get_gain(1, range1);     // LSB/V
        const auto offset0 = get_offset(0, range0); // LSB
        const auto offset1 = get_offset(1, range1); // LSB

        return std::array{ (val0 - offset0) / gain0,
                           (val1 - offset1) / gain1 };
    }

    // Calibrations

    auto get_calibration(uint32_t channel) {
        if (channel >= 2) {
            ctx.log<ERROR>("Ltc2387::get_calibration: Invalid channel\n");
            return std::array<float, cal_coeffs_num>{};
        }

        return cal_coeffs[channel];
    }

    int32_t set_calibration(uint32_t channel, const std::array<float, cal_coeffs_num>& new_coeffs) {
        if (channel >= 2) {
            ctx.log<ERROR>("Ltc2387::set_calibration: Invalid channel\n");
            return -1;
        }

        cal_coeffs[channel] = new_coeffs;

        if (channel == 0) {
            static_assert(cal_coeffs_num * sizeof(float) <= eeprom_map::rf_adc_ch0_calib::range);
            return eeprom.write<eeprom_map::rf_adc_ch0_calib::offset>(cal_coeffs[0]);
        } else {
            static_assert(cal_coeffs_num * sizeof(float) <= eeprom_map::rf_adc_ch1_calib::range);
            return eeprom.write<eeprom_map::rf_adc_ch1_calib::offset>(cal_coeffs[1]);
        }
    }

    float get_gain(uint32_t channel, uint32_t range) const {
        if (channel >= 2) {
            ctx.log<ERROR>("Ltc2387::get_gain: Invalid channel\n");
            return NAN;
        }

        if (range == RANGE_2V) {
            return cal_coeffs[channel][0];
        } else if (range == RANGE_8V) {
            return cal_coeffs[channel][2];
        } else {
            ctx.log<ERROR>("Ltc2157::get_gain: Invalid range\n");
            return NAN;
        }
    }

    float get_offset(uint32_t channel, uint32_t range) const {
        if (channel >= 2) {
            ctx.log<ERROR>("Ltc2387::get_offset: Invalid channel\n");
            return NAN;
        }

        if (range == RANGE_2V) {
            return cal_coeffs[channel][1];
        } else if (range == RANGE_8V) {
            return cal_coeffs[channel][3];
        } else {
            ctx.log<ERROR>("Ltc2157::get_offset: Invalid range\n");
            return NAN;
        }
    }

  private:
    Context& ctx;
    Memory<mem::control>& ctl;
    Memory<mem::status>& sts;
    Eeprom& eeprom;
    ClockGenerator& clkgen;

    // Calibration array: [gain_2V, offset_2V, gain_8V, offset_8V]
    // gain in LSB / Volts
    // offset in LSB

    std::array<std::array<float, cal_coeffs_num>, 2> cal_coeffs;

    int32_t to_two_complement(int32_t data) {
        constexpr int32_t nmax = 262144; // 2^18
        constexpr int32_t nmax_half = nmax / 2;
        return (data + nmax_half) % nmax - nmax_half;
    }
};

#endif // __ALPHA15_DRIVERS_LTC2387_HPP__
