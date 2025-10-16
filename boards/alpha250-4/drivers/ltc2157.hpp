/// Ltc2157 driver
///
/// (c) Koheron

#ifndef __ALPHA250_4_DRIVERS_LTC2157_HPP__
#define __ALPHA250_4_DRIVERS_LTC2157_HPP__

#include <array>
#include <ranges>

#include <scicpp/core.hpp>
#include <scicpp/polynomials.hpp>

namespace {
    namespace sci = scicpp;
    namespace poly = scicpp::polynomial;
}

// http://cds.linear.com/docs/en/datasheet/21576514fb.pdf

class Eeprom;
class SpiConfig;

class Ltc2157
{
  public:
    Ltc2157();

    enum regs {
        RESET,
        POWER_DOWN,
        TIMING,
        OUTPUT_MODE,
        DATA_FORMAT
    };

    void init();
    const std::array<float, 8> get_calibration(uint32_t adc, uint32_t channel);
    int32_t set_calibration(uint32_t adc, uint32_t channel, const std::array<float, 8>& new_coeffs);

    template<typename T=float>
    auto tf_polynomial(uint32_t adc, uint32_t channel) {
        const auto idx = (adc << 1) + channel;
        // Keep only the last 6 elements of cal_coeffs in reverse order
        std::array<T, 6> coeffs{};
        std::ranges::reverse_copy(cal_coeffs[idx] | std::views::drop(2) | std::views::take(6), coeffs.begin());
        return coeffs;
    }

    template<uint32_t adc, uint32_t channel, uint32_t n_pts>
    auto get_inverse_transfer_function(float fs) {
        static_assert(adc < 2, "Invalid adc");
        static_assert(channel < 2, "Invalid channel");

        const auto f = sci::arange(0.0f, 0.5f * fs, 0.5f * fs / n_pts);
        return poly::polyval(f, tf_polynomial(adc, channel));
    }

    float get_input_voltage_range(uint32_t adc, uint32_t channel) const;
    float get_gain(uint32_t adc, uint32_t channel) const;
    float get_offset(uint32_t adc, uint32_t channel) const;

  private:
    Eeprom& eeprom;
    SpiConfig& spi_cfg;

    // Calibration array: [gain, offset, p0, p1, p2, p3, p4, p5]
    // gain in LSB / Volts
    // offset in LSB
    // pi: Coefficient of the polynomial fit of 1 / H(f)

    std::array<std::array<float, 8>, 4> cal_coeffs;

    void write_reg(uint32_t data);
};

#endif // __ALPHA250_4_DRIVERS_LTC2157_HPP__
