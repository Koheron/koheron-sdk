#include "../moving_averager.hpp"
#include "../acquisition_window.hpp"
#include "../phase_scaling.hpp"
#include "../phase-processing.hpp"
#include "../tracking_lock.hpp"
#include "server/network/serializer_deserializer.hpp"

#include <cassert>
#include <deque>
#include <fstream>
#include <iostream>
#include <numeric>
#include <random>

void test_averager() {
    MovingAverager<float> avg(3);
    std::deque<float> expected;
    std::mt19937 random(42);
    std::size_t window = 3;
    for (int i = 0; i < 2000; ++i) {
        if (i % 7 == 0) {
            window = 1 + random() % 20;
            avg.set_navg(window);
            while (expected.size() > window) expected.pop_front();
        }
        if (i % 113 == 0) {
            avg.clear();
            expected.clear();
        }
        const float value = int(random() % 2000) - 1000.f;
        avg.append(std::vector<float>{value, -value});
        expected.push_back(value);
        if (expected.size() > window) expected.pop_front();
        const float mean = std::accumulate(expected.begin(), expected.end(), 0.f) / expected.size();
        assert(avg.count() == expected.size());
        assert(std::abs(avg.average()[0] - mean) < 1e-4f);
        assert(std::abs(avg.average()[1] + mean) < 1e-4f);
    }
}

void test_windows() {
    assert(!acquisition_window(7, 0, 8));
    auto first = *acquisition_window(8, 0, 8);
    assert(first.first_chunk == 0 && first.end_chunk == 8);
    assert(!acquisition_window(8, first.end_chunk, 8));
    assert(!acquisition_window(15, first.end_chunk, 8));
    assert(acquisition_window(16, first.end_chunk, 8)->first_chunk == 8);
    // A producer jump still selects a fresh window, including across a ring wrap.
    auto wrap = *acquisition_window(2050, 2040, 8);
    assert(wrap.first_chunk == 2042 && wrap.end_chunk == 2050);
    assert(wrap.first_chunk % 2048 == 2042 && (wrap.end_chunk - 1) % 2048 == 1);
    assert(acquisition_window_is_intact(wrap, 2051, 2048));
    assert(!acquisition_window_is_intact(wrap, wrap.first_chunk + 2048, 2048));
    assert(!acquisition_window(100, 108, 8)); // configuration settling barrier
}

void test_scaling() {
    for (uint32_t rate : {4u, 8u, 16u, 32u, 64u, 8192u})
        assert(cic_gain_compensation(rate, 6, 1) == 1.0);
    assert(std::abs(cic_gain_compensation(20, 6, 1) - 1.048576) < 1e-12);
    assert(std::abs(cic_gain_compensation(100, 6, 1) - 1.099511627776) < 1e-12);
    for (uint32_t rate = 4; rate <= 8192; ++rate) {
        const double compensation = cic_gain_compensation(rate, 6, 1);
        assert(compensation >= 1.0 && compensation < 2.0);
    }
    const double unity = uint32_t{1} << 30;
    for (double ref : {10e6, 10e6 + 0.637, 80e6, 31e6}) {
        for (double dut : {1e6, 10e6, 25e6, 80e6}) {
            const auto s = phase_scaling(dut, ref);
            assert(s.dut > 0 && s.reference > 0);
            assert(s.dut <= unity && s.reference <= unity);
            assert(std::abs(s.output_scale * s.dut / unity - 1.) < 1e-8);
            assert(std::abs(s.output_scale * s.reference / unity - dut / ref) < 1e-8);
            for (int32_t px : {-100000, 0, 100000}) {
                for (int32_t py : {-100000, 0, 100000}) {
                    const int64_t x = (int64_t(px) * s.dut) >> 30;
                    const int64_t y = (int64_t(py) * s.reference) >> 30;
                    const double actual = (x - y) * s.output_scale;
                    assert(std::abs(actual - (px - dut / ref * py)) <= 2 * s.output_scale);
                }
            }
        }
    }
    assert(phase_scaling(0., 10e6).dut == unity);
    assert(phase_scaling(10e6, 0.).reference == unity);
}

void test_phase_conversion_and_slope() {
    using Phase = scicpp::units::radian<float>;
    std::array<Phase, 32000> ramp{};
    for (std::size_t i = 0; i < ramp.size(); ++i) {
        ramp[i] = Phase{float(i) * 1e-4f + (i % 2 ? 1e-6f : -1e-6f)};
    }
    assert(std::abs(phase_slope_per_sample<32000>(ramp).eval() - 1e-4f) < 1e-9f);
    ramp.fill(Phase{});
    assert(phase_slope_per_sample<32000>(ramp).eval() == 0.0f);
    std::mt19937 random(23);
    std::normal_distribution<float> noise(0.0f, 0.001f);
    for (std::size_t i = 0; i < ramp.size(); ++i)
        ramp[i] = Phase{5.0f + float(i) * 1e-4f + noise(random)};
    assert(std::abs(phase_slope_per_sample<32000>(ramp).eval() - 1e-4f) < 2e-8f);
    // Preserve small increments despite large unwrap offsets.
    std::array<int32_t, 32000> raw{};
    for (std::size_t i = 0; i < raw.size(); ++i) raw[i] = 2000000000 + int32_t(i / 17);
    convert_relative_phase(raw, ramp, Phase{0.0015f});
    assert(ramp.front().eval() == 0.0f);
    assert(std::abs(phase_slope_per_sample<32000>(ramp).eval() - 0.0015f / 17.f) < 1e-9f);
    raw.fill(INT32_MIN);
    raw.back() = INT32_MAX;
    convert_relative_phase(raw, ramp, Phase{1.0f});
    assert(ramp.back().eval() > 4e9f); // subtraction must not overflow int32_t
    // The unsigned-magnitude path retains the original signed-difference result.
    for (auto& value : raw) value = int32_t(random());
    for (const int32_t origin : {INT32_MIN, 0, INT32_MAX}) {
        raw.front() = origin;
        convert_relative_phase(raw, ramp, Phase{1.0f});
        for (std::size_t i = 0; i < raw.size(); ++i)
            assert(ramp[i].eval() == float(int64_t(raw[i]) - int64_t(origin)));
    }
}

template<class Tuple>
void write_tuple(const char* path, Tuple tuple) {
    std::pmr::vector<unsigned char> bytes;
    net::CommandBuilder serializer;
    serializer.reset_into(bytes);
    serializer.push(tuple);
    std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

int main(int argc, char** argv) {
    TrackingLock lock;
    assert(!lock.update(1.0, 0.02, 0.005));
    for (int i = 0; i < 500; ++i) lock.update(i % 2 ? 0.03 : -0.03, 0.02, 0.005);
    assert(lock.update(0.0, 0.02, 0.005));
    assert(lock.update(0.05, 0.02, 0.005)); // an isolated noisy block is not unlock
    for (int i = 0; i < 50; ++i) lock.update(0.2, 0.02, 0.005);
    assert(!lock.update(0.2, 0.02, 0.005));
    lock.reset();
    assert(!lock.update(0.008, 0.02, 0.005)); // reset restores the tighter entry tolerance
    test_averager();
    test_windows();
    test_scaling();
    test_phase_conversion_and_slope();
    if (argc == 3) {
        using Phase = scicpp::units::radian<float>;
        using Time = scicpp::units::time<double>;
        using Frequency = scicpp::units::frequency<double>;
        write_tuple(argv[1], std::tuple{Phase{0.001f}, Time{1e-11}, Frequency{100.}, Frequency{1e6}, -3.});
        write_tuple(argv[2], std::tuple{true, Frequency{0.1}, Frequency{0.025}, Frequency{0.637},
                                      Frequency{-0.125}, Phase{0.01f}, Frequency{0.002}, true});
    }
    std::cout << "Averager, DMA-window and fixed-point scaling tests passed\n";
}
