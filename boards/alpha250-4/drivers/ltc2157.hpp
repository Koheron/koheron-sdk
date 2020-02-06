/// Ltc2157 driver
///
/// (c) Koheron

#ifndef __ALPHA250_4_DRIVERS_LTC2157_HPP__
#define __ALPHA250_4_DRIVERS_LTC2157_HPP__

#include <context.hpp>

#include <boards/alpha250/drivers/eeprom.hpp>
#include <boards/alpha250/drivers/spi-config.hpp>

#include <array>
#include <cmath>
#include <limits>

// http://cds.linear.com/docs/en/datasheet/21576514fb.pdf

class Ltc2157
{
  public:
    Ltc2157(Context& ctx_)
    : ctx(ctx_)
    , eeprom(ctx.get<Eeprom>())
    , spi_cfg(ctx.get<SpiConfig>())
    {}

    enum regs {
        RESET,
        POWER_DOWN,
        TIMING,
        OUTPUT_MODE,
        DATA_FORMAT
    };

    void init() {
        // Load calibrations
        eeprom.read<eeprom_map_alpha250_4::rf_adc0_ch0_calib::offset>(cal_coeffs[0]);
        eeprom.read<eeprom_map_alpha250_4::rf_adc0_ch1_calib::offset>(cal_coeffs[1]);
        eeprom.read<eeprom_map_alpha250_4::rf_adc1_ch0_calib::offset>(cal_coeffs[2]);
        eeprom.read<eeprom_map_alpha250_4::rf_adc1_ch1_calib::offset>(cal_coeffs[3]);

        spi_cfg.lock();

        // Reset
        write_reg((RESET << 8) + (1 << 7));

        // Power down
        //uint32_t SLEEP = 0;
        //uint32_t NAP = 0;
        //uint32_t PDB = 0;
        //write_reg((POWER_DOWN << 8) + (SLEEP << 3) + (NAP << 2) + (PDB << 1));

        // Output mode
        uint32_t ILVDS = 0b111; // 1.75 mA
        uint32_t TERMON = 1;
        uint32_t OUTOFF = 0;
        write_reg((OUTPUT_MODE << 8) + (ILVDS << 2) + (TERMON << 1) + (OUTOFF << 0));

        // Timing
        //uint32_t DELAY = 0;
        //uint32_t DCS = 0;
        //write_reg((TIMING << 8) + (DELAY << 1) + (DCS << 0));

        // Data format
        uint32_t RAND = 1;
        uint32_t TWOSCOMP = 1;
        write_reg((DATA_FORMAT << 8) + (RAND << 1) + (TWOSCOMP << 0));
        spi_cfg.unlock();
    }

    auto get_calibration(uint32_t adc, uint32_t channel) {
        if (adc >= 2) {
            ctx.log<ERROR>("Ltc2157::get_calibration: Invalid adc\n");
            return std::array<float, 8>{};
        }

        if (channel >= 2) {
            ctx.log<ERROR>("Ltc2157::get_calibration: Invalid channel\n");
            return std::array<float, 8>{};
        }

        return cal_coeffs[(adc << 1) + channel];
    }

    int32_t set_calibration(uint32_t adc, uint32_t channel, const std::array<float, 8>& new_coeffs) {
        if (adc >= 2) {
            ctx.log<ERROR>("Ltc2157::set_calibration: Invalid adc\n");
            return -1;
        }

        if (channel >= 2) {
            ctx.log<ERROR>("Ltc2157::set_calibration: Invalid channel\n");
            return -1;
        }

        cal_coeffs[(adc << 1) + channel] = new_coeffs;

        if (adc == 0) {
            if (channel == 0) {
                static_assert(8 * sizeof(float) <= eeprom_map_alpha250_4::rf_adc0_ch0_calib::range, "");
                return eeprom.write<eeprom_map_alpha250_4::rf_adc0_ch0_calib::offset>(cal_coeffs[0]);
            } else {
                static_assert(8 * sizeof(float) <= eeprom_map_alpha250_4::rf_adc0_ch1_calib::range, "");
                return eeprom.write<eeprom_map_alpha250_4::rf_adc0_ch1_calib::offset>(cal_coeffs[1]);
            }
        } else {
            if (channel == 0) {
                static_assert(8 * sizeof(float) <= eeprom_map_alpha250_4::rf_adc1_ch0_calib::range, "");
                return eeprom.write<eeprom_map_alpha250_4::rf_adc1_ch0_calib::offset>(cal_coeffs[0]);
            } else {
                static_assert(8 * sizeof(float) <= eeprom_map_alpha250_4::rf_adc1_ch1_calib::range, "");
                return eeprom.write<eeprom_map_alpha250_4::rf_adc1_ch1_calib::offset>(cal_coeffs[1]);
            }
        }
    }

    template<uint32_t adc, uint32_t channel, uint32_t n_pts>
    auto get_inverse_transfer_function(float fs) {
        static_assert(adc < 2, "Invalid adc");
        static_assert(channel < 2, "Invalid channel");

        std::array<float, n_pts> res;
        constexpr auto idx = (adc << 1) + channel;

        for (unsigned int i=0; i<n_pts; i++) {
            const float f = i * fs / (2 * n_pts);
            res[i] = cal_coeffs[idx][2] * f * f * f * f * f
                   + cal_coeffs[idx][3] * f * f * f * f
                   + cal_coeffs[idx][4] * f * f * f
                   + cal_coeffs[idx][5] * f * f
                   + cal_coeffs[idx][6] * f
                   + cal_coeffs[idx][7];
        }

        return res;
    }

    float get_input_voltage_range(uint32_t adc, uint32_t channel) const {
        float g = get_gain(adc, channel);

        if (std::isnan(g) || std::abs(g) < std::numeric_limits<float>::epsilon()) {
            return NAN;
        }

        return (2 << 15) / g;
    }

    float get_gain(uint32_t adc, uint32_t channel) const {
        if (adc >= 2) {
            ctx.log<ERROR>("Ltc2157::get_gain: Invalid adc\n");
            return NAN;
        }

        if (channel >= 2) {
            ctx.log<ERROR>("Ltc2157::get_gain: Invalid channel\n");
            return NAN;
        }

        return cal_coeffs[(adc << 1) + channel][0];
    }

    float get_offset(uint32_t adc, uint32_t channel) const {
        if (adc >= 2) {
            ctx.log<ERROR>("Ltc2157::get_offset: Invalid adc\n");
            return NAN;
        }

        if (channel >= 2) {
            ctx.log<ERROR>("Ltc2157::get_offset: Invalid channel\n");
            return NAN;
        }

        return cal_coeffs[(adc << 1) + channel][1];
    }

  private:
    Context& ctx;
    Eeprom& eeprom;
    SpiConfig& spi_cfg;

    // Calibration array: [gain, offset, p0, p1, p2, p3, p4, p5]
    // gain in LSB / Volts
    // offset in LSB
    // pi: Coefficient of the polynomial fit of 1 / H(f)

    std::array<std::array<float, 8>, 4> cal_coeffs;

    void write_reg(uint32_t data) {
        spi_cfg.write_reg<1, 2>(data << 16); // ADC0 config
        spi_cfg.write_reg<2, 2>(data << 16); // ADC1 config
    }
};

#endif // __ALPHA250_4_DRIVERS_LTC2157_HPP__
