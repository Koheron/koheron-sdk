// Probe AMD's locally installed multiplier and CORDIC models. Vendor files
// remain in tmp; none are redistributed with this regression.
#include "cmpy_v6_0_bitacc_cmodel.h"
#include "cordic_v6_0_bitacc_cmodel.h"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

static void message(void*, int error, const char* text) {
    if (error) std::fprintf(stderr, "%s\n", text);
}

int main(int argc, char** argv) {
    if (argc != 4) return 1;
    const int width = std::atoi(argv[1]);
    if (width != 16 && width != 24) return 1;
    auto input_file = std::fopen(argv[2], "rb");
    if (!input_file) return 2;
    std::fseek(input_file, 0, SEEK_END);
    const auto bytes = std::ftell(input_file);
    if (bytes <= 0 || bytes % (4 * sizeof(int32_t))) return 2;
    const size_t count = bytes / (4 * sizeof(int32_t));
    std::rewind(input_file);
    std::vector<int32_t> samples(count * 4);
    if (std::fread(samples.data(), sizeof(int32_t), count * 4, input_file) != count * 4) return 3;
    std::fclose(input_file);

    const auto complex_array = [count] {
        auto array = xip_array_complex_create();
        xip_array_complex_reserve_dim(array, 1);
        array->dim_size = 1;
        array->dim[0] = array->data_size = count;
        xip_array_complex_reserve_data(array, count);
        return array;
    };
    const auto real_array = [count] {
        auto array = xip_array_real_create();
        xip_array_real_reserve_dim(array, 1);
        array->dim_size = 1;
        array->dim[0] = array->data_size = count;
        xip_array_real_reserve_data(array, count);
        return array;
    };
    auto adc = complex_array(), lo = complex_array(), mixed = complex_array(), filtered = complex_array();
    auto magnitude = real_array(), phase = real_array();
    auto control = xip_array_uint_create();
    xip_array_uint_reserve_dim(control, 1);
    control->dim_size = 1;
    control->dim[0] = control->data_size = count;
    xip_array_uint_reserve_data(control, count);
    for (size_t i = 0; i < count; ++i) {
        adc->data[i] = {double(samples[4*i]), 0};
        lo->data[i] = {double(samples[4*i+1]), double(samples[4*i+2])};
        control->data[i] = uint32_t(samples[4*i+3]) & 1;
    }

    xip_cmpy_v6_0_config mixer_config{};
    xip_cmpy_v6_0_default_config(&mixer_config);
    mixer_config.DataType = 0;
    mixer_config.APortWidth = mixer_config.BPortWidth = 16;
    mixer_config.OutputWidth = width;
    mixer_config.RoundMode = 1; // Random_Rounding: randomize exact ties only.
    mixer_config.debug = 0;
    auto mixer = xip_cmpy_v6_0_create(&mixer_config, message, nullptr);
    if (!mixer) return 4;
    if (xip_cmpy_v6_0_data_do(mixer, adc, lo, control, mixed) != XIP_STATUS_OK) return 5;

    // Independent direct convolution, also used by the separate RTL test.
    int taps[61]{};
    for (int a=0; a<16; ++a) for (int b=0; b<16; ++b)
        for (int c=0; c<16; ++c) for (int d=0; d<16; ++d) ++taps[a+b+c+d];
    for (size_t i = 0; i < count; ++i) {
        int64_t re = 0, im = 0;
        for (size_t j = 0; j <= 60 && j <= i; ++j) {
            re += int64_t(mixed->data[i-j].re) * taps[j];
            im += int64_t(mixed->data[i-j].im) * taps[j];
        }
        const auto random = uint32_t(samples[4*i+3]);
        filtered->data[i] = {double((re + ((random >> 1) & 65535)) >> 16),
                             double((im + ((random >> 16) & 65535)) >> 16)};
    }

    xip_cordic_v6_0_config cordic_config{};
    xip_cordic_v6_0_default_config(&cordic_config);
    cordic_config.CordicFunction = 1;
    cordic_config.CoarseRotate = 1;
    cordic_config.DataFormat = 0;
    cordic_config.PhaseFormat = 1; // Scaled radians.
    cordic_config.InputWidth = width;
    cordic_config.OutputWidth = 24;
    cordic_config.Iterations = cordic_config.Precision = 0;
    cordic_config.RoundMode = 2;
    cordic_config.ScaleComp = 0;
    cordic_config.debug = 0;
    auto cordic = xip_cordic_v6_0_create(&cordic_config, message, nullptr);
    if (!cordic) return 6;
    if (xip_cordic_v6_0_translate(cordic, filtered, magnitude, phase, count) != XIP_STATUS_OK) return 7;
    auto output = std::fopen(argv[3], "wb");
    if (!output) return 8;
    constexpr double pi = 3.14159265358979323846;
    for (size_t i = 0; i < count; ++i) {
        const double radians = phase->data[i] * pi / (1 << 21);
        if (std::fwrite(&radians, sizeof radians, 1, output) != 1) return 8;
    }
    std::fclose(output);
    xip_cordic_v6_0_destroy(cordic);
    xip_cmpy_v6_0_destroy(mixer);
    for (auto array : {adc, lo, mixed, filtered}) xip_array_complex_destroy(array);
    for (auto array : {magnitude, phase}) xip_array_real_destroy(array);
    xip_array_uint_destroy(control);
}
