/// Redpitaya ADC calibration
///
/// (c) Koheron

#ifndef __REDPITAYA_DRIVERS_ADC_CALIBRATION_HPP__
#define __REDPITAYA_DRIVERS_ADC_CALIBRATION_HPP__

#include <context.hpp>

#include <array>
#include <ranges>
#include <cmath>
#include <limits>

#include <scicpp/core.hpp>
#include <scicpp/polynomials.hpp>

namespace {
    namespace sci = scicpp;
    namespace poly = scicpp::polynomial;
}

// Calibration array: [gain, offset, p0, p1, p2, p3, p4, p5]
// gain in LSB / Volts
// offset in LSB
// pi: Coefficient of the polynomial fit of 1 / H(f)

static constexpr auto cal_coeffs = std::array{
    std::array{7.25926074e+03f, -1.25870003e+02f, 3.49261787e-38f,
               -4.39604832e-30f, 1.90004015e-22f, -3.35273936e-15f,
               2.54795065e-08f,  9.48362052e-01f},

    std::array{7.26281201e+03f, -7.75899963e+01f, 4.63291048e-38f,
               -5.88647924e-30f, 2.58020095e-22f, -4.58396909e-15f,
               3.54267726e-08f,  9.28313494e-01f}
};

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
        // Keep only the last 6 elements of cal_coeffs in reverse order
        std::array<float, 6> coeffs{};
        std::ranges::reverse_copy(cal_coeffs[channel] | std::views::drop(2) | std::views::take(6), coeffs.begin());
        const auto f = sci::arange(0.0f, 0.5f * fs, 0.5f * fs / n_pts);
        return poly::polyval(f, coeffs);
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
