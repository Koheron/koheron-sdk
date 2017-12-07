/// Redpitaya ADC calibration
///
/// (c) Koheron

#ifndef __REDPITAYA_DRIVERS_ADC_CALIBRATION_HPP__
#define __REDPITAYA_DRIVERS_ADC_CALIBRATION_HPP__

#include <context.hpp>

#include <array>
#include <cmath>
#include <limits>

// Calibration array: [gain, offset, p0, p1, p2, p3, p4, p5]
// gain in LSB / Volts
// offset in LSB
// pi: Coefficient of the polynomial fit of 1 / H(f)

static constexpr std::array<std::array<float, 8>, 2> cal_coeffs = {{
    {7.25926074e+03, -1.25870003e+02, 3.49261787e-38, -4.39604832e-30, 1.90004015e-22, -3.35273936e-15, 2.54795065e-08, 9.48362052e-01},
    {7.26281201e+03, -7.75899963e+01, 4.63291048e-38, -5.88647924e-30, 2.58020095e-22, -4.58396909e-15, 3.54267726e-08, 9.28313494e-01}
}};

class RedPitayaAdcCalibration
{
  public:
    RedPitayaAdcCalibration(Context& ctx_)
    : ctx(ctx_)
    {}

    const auto get_calibration(uint32_t channel) {
        if (channel >= 2) {
            ctx.log<ERROR>("RedPitayaAdcCalibration::get_calibration: Invalid channel\n");
            return std::array<float, 8>{};
        }

        return cal_coeffs[channel];
    }

    template<uint32_t channel, uint32_t n_pts>
    const auto get_inverse_transfer_function(float fs) {
        static_assert(channel < 2, "Invalid channel");
        std::array<float, n_pts> res;

        for (unsigned int i=0; i<n_pts; i++) {
            float f = i * fs / (2 * n_pts);
            res[i] = cal_coeffs[channel][2] * f * f * f * f * f
                   + cal_coeffs[channel][3] * f * f * f * f
                   + cal_coeffs[channel][4] * f * f * f
                   + cal_coeffs[channel][5] * f * f
                   + cal_coeffs[channel][6] * f
                   + cal_coeffs[channel][7];
        }

        return res;
    }

    float get_input_voltage_range(uint32_t channel) const {
        float g = get_gain(channel);

        if (std::isnan(g) || std::abs(g) < std::numeric_limits<float>::epsilon()) {
            return NAN;
        }

        return (1 << 14) / g;
    }

    float get_gain(uint32_t channel) const {
        if (channel >= 2) {
            ctx.log<ERROR>("RedPitayaAdcCalibration::get_gain: Invalid channel\n");
            return NAN;
        }

        return cal_coeffs[channel][0];
    }

    float get_offset(uint32_t channel) const {
        if (channel >= 2) {
            ctx.log<ERROR>("RedPitayaAdcCalibration::get_offset: Invalid channel\n");
            return NAN;
        }

        return cal_coeffs[channel][1];
    }

  private:
    Context& ctx;
};

#endif // __REDPITAYA_DRIVERS_ADC_CALIBRATION_HPP__
