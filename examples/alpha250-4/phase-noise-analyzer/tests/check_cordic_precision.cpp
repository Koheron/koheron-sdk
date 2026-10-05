#include "cordic_v6_0_bitacc_cmodel.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>

static void message(void*, int, const char* text) {
    std::fprintf(stderr, "%s\n", text);
}

int main(int argc, char** argv) {
    if (argc != 6) return 2;
    const int width = std::atoi(argv[1]);
    const int precision = std::atoi(argv[2]);
    const int count = std::atoi(argv[3]);
    const double amplitude = std::atof(argv[4]);
    constexpr double pi = 3.14159265358979323846;

    xip_cordic_v6_0_config config{};
    xip_cordic_v6_0_default_config(&config);
    config.CordicFunction = 1; // Translate Cartesian I/Q to magnitude/phase.
    config.CoarseRotate = 1;
    config.DataFormat = 0;
    config.PhaseFormat = 1; // Scaled radians.
    config.InputWidth = 16;
    config.OutputWidth = width;
    config.Iterations = 0;
    config.Precision = precision;
    config.RoundMode = 2; // Round_Pos_Neg_Inf, matching the FPGA IP.
    config.ScaleComp = 0;
    auto model = xip_cordic_v6_0_create(&config, message, nullptr);
    if (!model) return 3;
    auto input = xip_array_complex_create();
    auto magnitude = xip_array_real_create();
    auto phase = xip_array_real_create();
    xip_array_complex_reserve_dim(input, 1);
    input->dim_size = 1;
    input->dim[0] = count;
    input->data_size = count;
    xip_array_complex_reserve_data(input, count);
    for (auto array : {magnitude, phase}) {
        xip_array_real_reserve_dim(array, 1);
        array->dim_size = 1;
        array->dim[0] = count;
        array->data_size = count;
        xip_array_real_reserve_data(array, count);
    }
    for (int i = 0; i < count; ++i) {
        const double angle = 2 * pi * i / count;
        input->data[i] = {std::round(amplitude * std::cos(angle)),
                          std::round(amplitude * std::sin(angle))};
    }
    if (xip_cordic_v6_0_translate(model, input, magnitude, phase, count) != XIP_STATUS_OK)
        return 4;
    std::ofstream output(argv[5], std::ios::binary);
    for (int i = 0; i < count; ++i) {
        const double calculated = phase->data[i] * pi / std::ldexp(1., width - 3);
        const double reference = std::atan2(input->data[i].im, input->data[i].re);
        const double error = std::remainder(calculated - reference, 2 * pi);
        output.write(reinterpret_cast<const char*>(&error), sizeof error);
    }
    xip_array_complex_destroy(input);
    xip_array_real_destroy(magnitude);
    xip_array_real_destroy(phase);
    xip_cordic_v6_0_destroy(model);
    return output ? 0 : 5;
}
