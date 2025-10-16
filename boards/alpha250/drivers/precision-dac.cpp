#include "./precision-dac.hpp"
#include "./eeprom.hpp"

#include "server/runtime/syslog.hpp"
#include "server/runtime/driver_manager.hpp"
#include "server/hardware/memory_manager.hpp"

#include <cmath>

PrecisionDac::PrecisionDac()
: eeprom(rt::get_driver<Eeprom>())
{}

void PrecisionDac::init() {
    eeprom.read<eeprom_map::precision_dac_calib::offset>(cal_coeffs);
    auto& ctl = hw::get_memory<mem::control>();
    ctl.write<reg::precision_dac_ctl>((regs::RESET << 1));
    ctl.write<reg::precision_dac_ctl>((regs::WRITE_UPDATE << 1) + enable);
    set_dac_value(0, 0);
    set_dac_value(1, 0);
    set_dac_value(2, 0);
    set_dac_value(3, 0);
}

void PrecisionDac::set_dac_value_volts(uint32_t channel, float voltage) {
    if (channel >= n_dacs) {
        log<ERROR>("PrecisionDac::set_dac_value_volts invalid channel");
        return;
    }

    if (std::isnan(voltage)) {
        log<ERROR>("PrecisionDac::set_dac_value_volts voltage is NaN");
        return;
    }

    if (voltage > 2.5f) {
        voltage = 2.5f;
    }

    if (voltage < 0.0f) {
        voltage = 0.0f;
    }

    values_volt[channel] = voltage;

    uint32_t code = uint32_t(std::round(cal_coeffs[2 * channel] * voltage + cal_coeffs[2 * channel + 1]));
    set_dac_value(channel, code);
}

void PrecisionDac::set_dac_value(uint32_t channel, uint32_t code) {
    dac_values[channel & 0b11] = code;
    auto& ctl = hw::get_memory<mem::control>();
    ctl.write<reg::precision_dac_data0>((dac_values[1] << 16) + (dac_values[0] & 0xFFFF));
    ctl.write<reg::precision_dac_data1>((dac_values[3] << 16) + (dac_values[2] & 0xFFFF));
}

int32_t PrecisionDac::set_calibration_coeffs(const std::array<float, 2 * n_dacs>& new_coeffs) {
    cal_coeffs = new_coeffs;
    static_assert(2 * n_dacs * sizeof(float) <= eeprom_map::precision_dac_calib::range, "");
    return eeprom.write<eeprom_map::precision_dac_calib::offset>(cal_coeffs);
}