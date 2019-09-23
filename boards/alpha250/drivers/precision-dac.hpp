#ifndef __ALPHA_DRIVERS_PRECISION_DAC_HPP__
#define __ALPHA_DRIVERS_PRECISION_DAC_HPP__

#include <context.hpp>

#include <array>
#include <cmath>

#include "eeprom.hpp"

static constexpr uint32_t n_dacs = 4;

class PrecisionDac
{
  public:
    PrecisionDac(Context& ctx_)
    : ctx(ctx_)
    , ctl(ctx.mm.get<mem::control>())
    , eeprom(ctx.get<Eeprom>())
    {}

    enum regs {
        NO_OPERATION,
        WRITE,
        UPDATE,
        WRITE_UPDATE,
        POWER_UP_DOWN,
        LDAC_MASK,
        RESET,
        RESERVED,
        DCEN,
        READBACK
    };

    void init() {
        eeprom.read<eeprom_map::precision_dac_calib::offset>(cal_coeffs);

        ctl.write<reg::precision_dac_ctl>((regs::RESET << 1));
        ctl.write<reg::precision_dac_ctl>((regs::WRITE_UPDATE << 1) + enable);
        set_dac_value(0, 0);
        set_dac_value(1, 0);
        set_dac_value(2, 0);
        set_dac_value(3, 0);
    }

    void set_dac_value_volts(uint32_t channel, float voltage) {
        if (channel >= n_dacs) {
            ctx.log<ERROR>("PrecisionDac::set_dac_value_volts invalid channel");
            return;
        }

        if (std::isnan(voltage)) {
            ctx.log<ERROR>("PrecisionDac::set_dac_value_volts voltage is NaN");
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

    void set_dac_value(uint32_t channel, uint32_t code) {
        dac_values[channel & 0b11] = code;
        ctl.write<reg::precision_dac_data0>((dac_values[1] << 16) + (dac_values[0] & 0xFFFF));
        ctl.write<reg::precision_dac_data1>((dac_values[3] << 16) + (dac_values[2] & 0xFFFF));
    }

    auto get_dac_values() const {
        return values_volt;
    }

    int32_t set_calibration_coeffs(const std::array<float, 2 * n_dacs>& new_coeffs) {
        cal_coeffs = new_coeffs;
        static_assert(2 * n_dacs * sizeof(float) <= eeprom_map::precision_dac_calib::range, "");
        return eeprom.write<eeprom_map::precision_dac_calib::offset>(cal_coeffs);
    }

  private:
    Context& ctx;
    Memory<mem::control>& ctl;
    Eeprom& eeprom;

    std::array<uint32_t, n_dacs> dac_values;
    std::array<float, n_dacs> values_volt;
    uint32_t enable = 1;

    // Calibration coefficients
    std::array<float, 2 * n_dacs> cal_coeffs;
};

#endif // __ALPHA_DRIVERS_PRECISION_DAC_HPP__