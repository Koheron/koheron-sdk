#ifndef __ALPHA250_DRIVERS_PRECISION_DAC_HPP__
#define __ALPHA250_DRIVERS_PRECISION_DAC_HPP__

#include <array>
#include <cstdint>

static constexpr uint32_t n_dacs = 4;

class Context;
class Eeprom;

class PrecisionDac
{
  public:
    PrecisionDac(Context& ctx_);
    void init();
    void set_dac_value_volts(uint32_t channel, float voltage);
    void set_dac_value(uint32_t channel, uint32_t code);

    auto get_dac_values() const {
        return values_volt;
    };

    int32_t set_calibration_coeffs(const std::array<float, 2 * n_dacs>& new_coeffs);

  private:
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

    Context& ctx;
    Eeprom& eeprom;

    std::array<uint32_t, n_dacs> dac_values{};
    std::array<float, n_dacs> values_volt{};
    uint32_t enable = 1;

    // Calibration coefficients
    std::array<float, 2 * n_dacs> cal_coeffs{};
};

#endif // __ALPHA250_DRIVERS_PRECISION_DAC_HPP__