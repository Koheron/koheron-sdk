#include "./ltc2387.hpp"
#include "./clock-generator.hpp"
#include "./eeprom.hpp"

#include "server/runtime/services.hpp"
#include "server/runtime/syslog.hpp"
#include "server/runtime/drivers_manager.hpp"

#include <cmath>
#include <chrono>
#include <thread>
#include <limits>

using services::require;

Ltc2387::Ltc2387()
: ctl   (require<hw::MemoryManager>().get<mem::control>())
, sts   (require<hw::MemoryManager>().get<mem::status>())
, eeprom(require<rt::DriverManager>().get<Eeprom>())
{}

void Ltc2387::init() {
    // Load calibrations
    eeprom.read<eeprom_map::rf_adc_ch0_calib::offset>(cal_coeffs[0]);
    eeprom.read<eeprom_map::rf_adc_ch1_calib::offset>(cal_coeffs[1]);

    // Clock generator must be initialized before enabling ADC
    enable_adcs();
    range_select(0, 0);
    range_select(1, 0);
    set_clock_delay();
}

// Data

std::array<int32_t, 2> Ltc2387::adc_raw_data(uint32_t n_avg) {
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

std::array<float, 2> Ltc2387::adc_data_volts(uint32_t n_avg) {
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

// Clock Delay

void Ltc2387::set_clock_delay() {
    using namespace std::literals;

    log("Ltc2387: Setting ADC clock delay ...\n");
    const auto t1 = std::chrono::high_resolution_clock::now();

    // Two lane mode test pattern
    constexpr int32_t testpat = 0b110011000011111100;
    constexpr int32_t n_hyst = 10;

    set_testpat();
    std::this_thread::sleep_for(50us);

    auto& clkgen = require<rt::DriverManager>().get<ClockGenerator>();
    int n = n_hyst;
    int32_t start = 0;
    int32_t end = 0;

    // Make sure we start from an unvalid zone
    while (n > 0) {
        clkgen.phase_shift(10);
        const auto [data0, data1] = adc_raw_data(1U);
        if (data0 != testpat || data1 != testpat) {
            n--;
        } else {
            n = n_hyst;
        }
    }

    // Find start of the valid window
    n = n_hyst;
    while (n > 0) {
        clkgen.phase_shift(10);
        const auto [data0, data1] = adc_raw_data(1U);
        if (data0 == testpat && data1 == testpat) {
            if (n == n_hyst) {
                start = clkgen.get_total_phase_shift();
            }
            n--;
        } else {
            n = n_hyst;
        }
    }

    // Find end of the valid window
    n = n_hyst;
    while (n > 0) {
        clkgen.phase_shift(10);
        const auto [data0, data1] = adc_raw_data(1U);
        if (data0 != testpat || data1 != testpat) {
            if (n == n_hyst) {
                end = clkgen.get_total_phase_shift();
            }
            n--;
        } else {
            n = n_hyst;
        }
    }

    // Go back to the middle of the valid window
    clkgen.phase_shift((end + start) / 2 - clkgen.get_total_phase_shift());
    while (!(sts.read_bit<reg::mmcm_sts, 1>())) {}

    const auto [data0, data1] = adc_raw_data(1U);
    logf("Ltc2387: testpat = {:#07x}, data0 = {:#07x}, data1 = {:#07x}\n",
         testpat, data0, data1);
    logf("Ltc2387: total phase shift = {}, window size = {}\n",
         clkgen.get_total_phase_shift(), end - start);

    if (data0 != testpat || data1 != testpat) {
        log<ERROR>("Ltc2387: Failed to set clock delay");
    }

    const auto t2 = std::chrono::high_resolution_clock::now();
    const auto duration = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1);
    logf("Ltc2387: Delay clock adjustment duration {:%Q %q}\n", duration);

    clear_testpat();
}

// reg::rf_adc_ctl
// BIT  0      : RANGE
// BIT  1      : TESTPAT
// BIT  2      : ENABLE
// BIT  3      : CLKOUT_DEC (Only ADC0 is used)

void Ltc2387::enable_adcs() {
    ctl.set_bit<reg::rf_adc_ctl0, 2>();
    ctl.set_bit<reg::rf_adc_ctl1, 2>();
}

void Ltc2387::disable_adcs() {
    ctl.clear_bit<reg::rf_adc_ctl0, 2>();
    ctl.clear_bit<reg::rf_adc_ctl1, 2>();
}

void Ltc2387::set_testpat() {
    ctl.set_bit<reg::rf_adc_ctl0, 1>();
    ctl.set_bit<reg::rf_adc_ctl1, 1>();
}

void Ltc2387::clear_testpat() {
    ctl.clear_bit<reg::rf_adc_ctl0, 1>();
    ctl.clear_bit<reg::rf_adc_ctl1, 1>();
}

void Ltc2387::clkout_dec() {
    ctl.set_bit<reg::rf_adc_ctl0, 3>();
    ctl.clear_bit<reg::rf_adc_ctl0, 3>();
}

// Input Range

void Ltc2387::range_select(uint32_t channel, uint32_t range) {
    // range = 0: Input range 2 Vpp
    // range = 1: Input range 8 Vpp

    if (range >= input_range_num) {
        log<ERROR>("Ltc2387::range_select: Invalid range\n");
        return;
    }

    if (channel >= 2) {
        log<ERROR>("Ltc2387::range_select: Invalid channel\n");
        return;
    }

    channel ? ctl.write_bit<reg::rf_adc_ctl1, 0>(range)
            : ctl.write_bit<reg::rf_adc_ctl0, 0>(range);

    logf("Ltc2387: Channel {} set to range {}\n", channel, range ? "8 Vpp": "2 Vpp");
}

uint32_t Ltc2387::input_range(uint32_t channel) {
    return channel ? ctl.read_bit<reg::rf_adc_ctl1, 0>()
                   : ctl.read_bit<reg::rf_adc_ctl0, 0>();
}

// Calibrations

std::array<float, cal_coeffs_num> Ltc2387::get_calibration(uint32_t channel) {
    if (channel >= 2) {
        log<ERROR>("Ltc2387::get_calibration: Invalid channel\n");
        return std::array<float, cal_coeffs_num>{};
    }

    return cal_coeffs[channel];
}

int32_t Ltc2387::set_calibration(uint32_t channel, const std::array<float, cal_coeffs_num>& new_coeffs) {
    if (channel >= 2) {
        log<ERROR>("Ltc2387::set_calibration: Invalid channel\n");
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

float Ltc2387::get_gain(uint32_t channel, uint32_t range) const {
    if (channel >= 2) {
        log<ERROR>("Ltc2387::get_gain: Invalid channel\n");
        return std::numeric_limits<float>::quiet_NaN();
    }

    if (range == RANGE_2V) {
        return cal_coeffs[channel][0];
    } else if (range == RANGE_8V) {
        return cal_coeffs[channel][2];
    } else {
        log<ERROR>("Ltc2157::get_gain: Invalid range\n");
        return std::numeric_limits<float>::quiet_NaN();
    }
}

float Ltc2387::get_offset(uint32_t channel, uint32_t range) const {
    if (channel >= 2) {
        log<ERROR>("Ltc2387::get_offset: Invalid channel\n");
        return std::numeric_limits<float>::quiet_NaN();
    }

    if (range == RANGE_2V) {
        return cal_coeffs[channel][1];
    } else if (range == RANGE_8V) {
        return cal_coeffs[channel][3];
    } else {
        log<ERROR>("Ltc2157::get_offset: Invalid range\n");
        return std::numeric_limits<float>::quiet_NaN();
    }
}

int32_t Ltc2387::to_two_complement(int32_t data) {
    constexpr int32_t nmax = 262144; // 2^18
    constexpr int32_t nmax_half = nmax / 2;
    return (data + nmax_half) % nmax - nmax_half;
}