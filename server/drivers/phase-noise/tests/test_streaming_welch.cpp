#include "server/drivers/phase-noise/streaming-welch.hpp"
#include "server/drivers/phase-noise/acquisition-window.hpp"
#include <fstream>
#include <iostream>
#include <bit>

template<typename T>
void check_same_spectrum(const std::vector<T>& actual, const std::vector<T>& expected) {
    assert(actual.size() == expected.size());
    double error = 0, norm = 0;
    for (std::size_t k = 0; k < actual.size(); ++k) {
        const double delta = std::abs(actual[k] - expected[k]);
        const double magnitude = std::abs(expected[k]);
        error += delta * delta; norm += magnitude * magnitude;
    }
    // Each workspace independently chooses its measured preparation kernel.
    // Both must agree within the existing independent SciPy oracle tolerance.
    assert(error <= 4e-12 * norm + 1e-60);
}

int main(int argc, char** argv) {
    assert(argc == 2);
    // Fusing normalization must retain DC/Nyquist scaling and gradual
    // underflow, including the ARMv7 NEON scalar fallback.
    for (const int simd : {1, 4}) for (const float normalization : {1e-39f, 1e-15f, 1e-3f, 1.f, 1e3f}) {
        std::array<float, 64> transformed{};
        const float values[]{0.f, 1e-22f, -1e-20f, 1e-10f, -1.f, 1e10f};
        for (std::size_t k = 0; k < transformed.size(); ++k) transformed[k] = values[k % 6];
        std::array<float, 33> original{}, fused{};
        phase_noise::detail::accumulate_welch_power(transformed.data(), original.data(), 64, simd);
        phase_noise::detail::accumulate_welch_power<false>(transformed.data(), fused.data(), 64, simd, normalization);
        for (std::size_t k = 0; k < original.size(); ++k) {
            const volatile float unscaled = original[k];
            const float expected = unscaled * (k == 0 || k == 32 ? normalization : 2.f * normalization);
            assert(std::bit_cast<uint32_t>(fused[k]) == std::bit_cast<uint32_t>(expected));
        }
    }
    constexpr std::size_t size = 512, hops = 12, samples = size + (hops - 1) * size / 2;
    std::array<int32_t, samples> x{}, y{};
    uint32_t random = 13;
    for (std::size_t i = 0; i < samples; ++i) {
        random ^= random << 13; random ^= random >> 17; random ^= random << 5;
        const double phase = 2 * scicpp::pi<double> * 19 * double(i) / size;
        const double tone = 200000 * std::sin(phase) + double(int32_t(random) % 10000);
        x[i] = int32_t(1900000000 + 50 * double(i) + tone);
        y[i] = int32_t(-1900000000 + 70 * double(i) - tone + 50000 * std::cos(phase));
    }
    std::ofstream raw(std::string(argv[1]) + ".raw", std::ios::binary);
    raw.write(reinterpret_cast<const char*>(x.data()), sizeof(x));
    raw.write(reinterpret_cast<const char*>(y.data()), sizeof(y));
    std::ofstream output(argv[1], std::ios::binary);
    phase_noise::StreamingWelch<size> single, paired, native_single, native_paired;
    std::vector<float> ordered_power;
    std::vector<std::complex<float>> ordered_cross;
    for (std::size_t hop = 0; hop < hops; ++hop) {
        const auto a = std::span<const int32_t>(x.data() + hop * size / 2, size);
        const auto b = std::span<const int32_t>(y.data() + hop * size / 2, size);
        single.process(a, 1e-5, 123456);
        paired.process(a, 1e-5, 123456, b, 1e-11); // Quiet channel: 120 dB power ratio.
        native_single.process(a, 1e-5, 123456, {}, 0, true, true);
        native_paired.process(a, 1e-5, 123456, b, 1e-11, true, true);
        native_single.order_to(native_single.density(), ordered_power);
        check_same_spectrum(ordered_power, single.density());
        native_paired.order_to(native_paired.latest_cross(), ordered_cross);
        check_same_spectrum(ordered_cross, paired.latest_cross());
        native_paired.order_to(native_paired.density(), ordered_power);
        check_same_spectrum(ordered_power, paired.density());
        assert(single.segment_count() == hop + 1);
        assert(paired.segment_count() == hop + 1);
        assert(single.retained_segments() == std::min(hop + 1, std::size_t{3}));
        for (std::size_t k = 0; k <= size / 2; ++k) {
            const float values[]{single.latest_power()[k], single.density()[k],
                paired.latest_cross()[k].real(), paired.latest_cross()[k].imag(), paired.density()[k]};
            output.write(reinterpret_cast<const char*>(values), sizeof(values));
        }
    }
    paired.reset();
    paired.process(std::span<const int32_t>(x.data(), size), 1e-5, 123456,
                   std::span<const int32_t>(y.data(), size), 1e-11);
    assert(paired.retained_segments() == 1 && paired.segment_count() == hops + 1);
    assert(paired.latest_cross()[19].real() < 0);
    const auto reference_cross = paired.latest_cross();
    paired.process(std::span<const int32_t>(x.data(), size), 1e-5, 123456,
                   std::span<const int32_t>(y.data(), size), 1e-11, false);
    assert(paired.latest_cross() == reference_cross && paired.segment_count() == hops + 2);

    // Production-sized windows cover fine steps on large offsets, almost
    // full-range carrier drift in both directions, and signed endpoint input.
    constexpr std::size_t edge_size = 32768;
    phase_noise::StreamingWelch<edge_size> edge_single, edge_paired;
    std::array<int32_t, edge_size> a{}, b{};
    std::ofstream edge_raw(std::string(argv[1]) + ".edge.raw", std::ios::binary);
    std::ofstream edge_output(std::string(argv[1]) + ".edge", std::ios::binary);
    for (int test = 0; test < 7; ++test) {
        for (std::size_t i = 0; i < edge_size; ++i) {
            const double tone = std::round(20 * std::sin(2 * scicpp::pi<double> * 113 * double(i) / edge_size));
            if (test == 0) {
                a[i] = 1900000000 + int((i * 19) % 37 < 17);
                b[i] = -1900000000 - int((i * 19) % 37 < 17);
            } else if (test < 3) {
                const double drift = -2000000000 + 122070.3125 * double(i);
                a[i] = int32_t((test == 1 ? drift : -drift) + tone);
                b[i] = int32_t((test == 1 ? -drift : drift) - tone);
            } else if (test == 4) {
                a[i] = int32_t(-1900000000 + 115987.123456 * double(i)) + int(i % 7 == 0);
                b[i] = -a[i];
            } else if (test == 5) {
                a[i] = i < edge_size / 2 ? INT32_MIN : INT32_MAX;
                b[i] = ~a[i];
            } else if (test == 6) {
                a[i] = i % 3 == 0 ? INT32_MAX : INT32_MIN;
                b[i] = ~a[i];
            } else {
                a[i] = i % 2 ? INT32_MAX : INT32_MIN;
                b[i] = i % 2 ? INT32_MIN : INT32_MAX;
            }
        }
        // Check preparation itself against the double reference, including
        // wide residuals and fractional slopes on large raw offsets.
        const auto trend = phase_noise::fit_raw_phase(a);
        const auto win = scicpp::signal::windows::hann<float>(edge_size);
        std::array<float, edge_size> prepared{};
        phase_noise::prepare_phase_window(a, trend, 1., win, prepared.data());
        for (std::size_t i = 0; i < edge_size; ++i) {
            const double expected = (double(a[i]) - (trend.anchor + trend.mean) -
                trend.slope * (double(i) - double(edge_size - 1) / 2)) * win[i];
            assert(std::abs(double(prepared[i]) - expected) <= 1e-4 + 3e-7 * std::abs(expected));
        }
        edge_raw.write(reinterpret_cast<const char*>(a.data()), sizeof(a));
        edge_raw.write(reinterpret_cast<const char*>(b.data()), sizeof(b));
        edge_single.process(a, 1e-5, 123456);
        edge_paired.process(a, 1e-5, 123456, b, 1e-11);
        for (std::size_t k = 0; k <= edge_size / 2; ++k) {
            const float values[]{edge_single.latest_power()[k],
                edge_paired.latest_cross()[k].real(), edge_paired.latest_cross()[k].imag()};
            edge_output.write(reinterpret_cast<const char*>(values), sizeof(values));
        }
    }

    auto first = streaming_acquisition_window(100, 10, 4, 2, false);
    assert(first && first->first_chunk == 10 && first->end_chunk == 14);
    auto next = streaming_acquisition_window(100, first->end_chunk, 4, 2, true);
    assert(next && next->first_chunk == 12 && next->end_chunk == 16);
    assert(!streaming_acquisition_window(15, 14, 4, 2, true));
    assert(!acquisition_window_is_intact(*first, 522, 512));
    std::cout << "Streaming Welch: persistent paired worker, signed quiet-channel CSD, counts, reset and sequential half-window scheduling passed\n";
}
