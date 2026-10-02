// Optional bit-accurate vendor-model check; see tests/README.md for dependencies.
#include "cordic_v6_0_bitacc_cmodel.h"
#include <cmath>
#include <cstdio>
#include <fstream>
#include <vector>

int main(int argc, char** argv) {
    if (argc != 2) return 1;
    constexpr size_t angles = 16384;
    constexpr size_t samples = 4 * angles;
    constexpr double pi = 3.14159265358979323846;
    constexpr double amplitude = 3084.5; // Representative demodulated I/Q counts.
    std::vector<double> errors(2 * angles);
    double rms[2]{};

    for (int mode = 0; mode < 2; ++mode) {
        const int bits = mode ? 24 : 16;
        const double scale = mode ? 256 : 1;
        xip_cordic_v6_0_config config;
        xip_cordic_v6_0_default_config(&config);
        config.CordicFunction = XIP_CORDIC_V6_0_F_TRANSLATE;
        config.CoarseRotate = 1;
        config.DataFormat = XIP_CORDIC_V6_0_FORMAT_SIG_FRAC;
        config.PhaseFormat = XIP_CORDIC_V6_0_FORMAT_SCA;
        config.InputWidth = bits;
        config.OutputWidth = bits;
        config.Iterations = 0;
        config.Precision = 0;
        config.RoundMode = XIP_CORDIC_V6_0_ROUND_POS_NEG_INF;
        config.ScaleComp = 0;
        auto core = xip_cordic_v6_0_create(&config, nullptr, nullptr);
        if (!core) return 2;

        auto input = xip_array_complex_create();
        xip_array_complex_reserve_dim(input, 1);
        input->dim_size = 1;
        input->dim[0] = input->data_size = samples;
        xip_array_complex_reserve_data(input, samples);
        auto magnitude = xip_array_real_create();
        auto phase = xip_array_real_create();
        for (auto output : {magnitude, phase}) {
            xip_array_real_reserve_dim(output, 1);
            output->dim_size = 1;
            output->dim[0] = output->data_size = samples;
            xip_array_real_reserve_data(output, samples);
        }

        // Compute all four possible independently rounded I/Q inputs. Weight
        // their phase results exactly, rather than estimating rounding bias
        // from a finite PRNG sequence. Keep the same physical input scale.
        for (size_t i = 0; i < angles; ++i) {
            const double angle = 2 * pi * (i + .37) / angles - pi;
            const double x = amplitude * scale * std::cos(angle);
            const double y = amplitude * scale * std::sin(angle);
            for (int corner = 0; corner < 4; ++corner) {
                input->data[4 * i + corner] = {
                    std::floor(x) + (corner & 1),
                    std::floor(y) + ((corner >> 1) & 1)
                };
            }
        }
        if (xip_cordic_v6_0_translate(core, input, magnitude, phase, samples)
                != XIP_STATUS_OK) return 3;

        for (size_t i = 0; i < angles; ++i) {
            const double angle = 2 * pi * (i + .37) / angles - pi;
            const double x = amplitude * scale * std::cos(angle);
            const double y = amplitude * scale * std::sin(angle);
            const double px = x - std::floor(x), py = y - std::floor(y);
            double error = 0;
            for (int corner = 0; corner < 4; ++corner) {
                const double probability = ((corner & 1) ? px : 1 - px)
                                         * ((corner & 2) ? py : 1 - py);
                const double actual = phase->data[4 * i + corner]
                                    * pi / std::pow(2, bits - 3);
                error += probability * std::remainder(actual - angle, 2 * pi);
            }
            errors[mode * angles + i] = error;
            rms[mode] += error * error;
        }
        rms[mode] = std::sqrt(rms[mode] / angles);
        std::printf("bits %d expected angle error RMS %.12g rad\n", bits, rms[mode]);
        xip_array_complex_destroy(input);
        xip_array_real_destroy(magnitude);
        xip_array_real_destroy(phase);
        xip_cordic_v6_0_destroy(core);
    }

    // New phase-count rounding has an exact unbiased mean (checked in RTL),
    // so its expected angle is the wide CORDIC's angle. These limits check
    // periodic extraction error, not the noise variance added by rounding.
    if (rms[0] < 5e-5 || rms[1] > 1e-6) return 4;
    std::ofstream output(argv[1], std::ios::binary);
    output.write(reinterpret_cast<const char*>(errors.data()), errors.size() * sizeof(double));
    return output ? 0 : 5;
}
