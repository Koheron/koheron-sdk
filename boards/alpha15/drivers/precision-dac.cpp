#include "./precision-dac.hpp"
#include "./eeprom.hpp"

#include "server/runtime/syslog.hpp"
#include "server/runtime/services.hpp"
#include "server/runtime/driver_manager.hpp"

#include <algorithm>
#include <cmath>

namespace {
bool valid_calibration(const std::array<float, 2 * n_dacs>& coeffs) {
    return std::all_of(coeffs.begin(), coeffs.end(), [](float value) { return std::isfinite(value); });
}
}

PrecisionDac::PrecisionDac()
: mm    (services::require<hw::MemoryManager>())
, eeprom(services::require<rt::DriverManager>().get<Eeprom>())
{}

void PrecisionDac::init() {
    std::array<float, 2 * n_dacs> loaded{};
    if (eeprom.read<eeprom_map::precision_dac_calib::offset>(loaded) == sizeof(loaded) &&
        valid_calibration(loaded)) {
        cal_coeffs = loaded;
        calibration_ready = true;
    } else {
        log<ERROR>("PrecisionDac: Cannot load valid calibration");
    }
    values_volt.fill(0.0f);
    auto& ctl = mm.get<mem::control>();
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

    if (!calibration_ready) {
        log<ERROR>("PrecisionDac::set_dac_value_volts requires calibration");
        return;
    }
    if (!(voltage >= 0.0f && voltage <= 2.5f)) {
        if (!std::isfinite(voltage)) {
            log<ERROR>("PrecisionDac::set_dac_value_volts requires a finite voltage");
            return;
        }
        voltage = std::clamp(voltage, 0.0f, 2.5f);
    }

    float bounded = cal_coeffs[2 * channel] * voltage + cal_coeffs[2 * channel + 1];
    if (!(bounded >= 0.0f && bounded <= 65535.0f)) {
        if (!std::isfinite(bounded)) {
            log<ERROR>("PrecisionDac::set_dac_value_volts calibration overflow");
            return;
        }
        // Clamp before conversion: negative/out-of-range floats must not wrap a DAC code.
        bounded = std::clamp(bounded, 0.0f, 65535.0f);
    }
    const auto code = static_cast<uint32_t>(std::lround(bounded));
    write_dac_value(channel, code);
    values_volt[channel] = voltage;
}

void PrecisionDac::set_dac_value(uint32_t channel, uint32_t code) {
    if (channel >= n_dacs || code > 0xFFFFu) {
        log<ERROR>("PrecisionDac::set_dac_value invalid channel or code");
        return;
    }
    write_dac_value(channel, code);
}

// Both callers validate the channel and 16-bit code before writing.
void PrecisionDac::write_dac_value(uint32_t channel, uint32_t code) {
    dac_values[channel] = code;
    auto& ctl = mm.get<mem::control>();
    // The SPI core samples these registers continuously; only this pair changed.
    if (channel < 2) {
        ctl.write<reg::precision_dac_data0>((dac_values[1] << 16) | dac_values[0]);
    } else {
        ctl.write<reg::precision_dac_data1>((dac_values[3] << 16) | dac_values[2]);
    }
}

int32_t PrecisionDac::set_calibration_coeffs(const std::array<float, 2 * n_dacs>& new_coeffs) {
    if (!valid_calibration(new_coeffs)) { return -1; }
    static_assert(2 * n_dacs * sizeof(float) <= eeprom_map::precision_dac_calib::range, "");
    const auto written = eeprom.write<eeprom_map::precision_dac_calib::offset>(new_coeffs);
    if (written != sizeof(new_coeffs)) { return -1; }
    cal_coeffs = new_coeffs;
    calibration_ready = true;
    return written;
}
