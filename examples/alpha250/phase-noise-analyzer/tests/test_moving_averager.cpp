#include "../moving_averager.hpp"

#include <cmath>
#include <deque>
#include <iostream>
#include <stdexcept>
#include <vector>

// A chronological queue provides an independent reference for ring resizing.
class Check {
    MovingAverager<double> actual;
    std::deque<std::vector<double>> samples;
    std::size_t capacity;

    void verify() const {
        const auto result = actual.average();
        if (actual.count() != samples.size() || actual.window() != capacity) {
            throw std::runtime_error("incorrect sample count or capacity");
        }
        if (samples.empty()) {
            if (!result.empty()) throw std::runtime_error("nonempty average after clear");
            return;
        }
        if (result.size() != 3) throw std::runtime_error("incorrect spectrum width");
        for (std::size_t bin = 0; bin < result.size(); ++bin) {
            double expected = 0;
            for (const auto& sample : samples) expected += sample[bin];
            expected /= samples.size();
            if (std::abs(result[bin] - expected) > 1e-10) {
                throw std::runtime_error("average differs from chronological reference");
            }
        }
    }

  public:
    explicit Check(std::size_t n) : actual(n), capacity(n) {}

    void append(double value) {
        std::vector<double> sample{value, -2 * value, value + 10};
        // Exercise both append overloads.
        if (samples.size() % 2) actual.append(std::vector<double>(sample));
        else actual.append(sample);
        samples.push_back(sample);
        if (samples.size() > capacity) samples.pop_front();
        verify();
    }

    void resize(std::size_t n) {
        actual.set_navg(n);
        capacity = std::max(std::size_t{1}, n);
        while (samples.size() > capacity) samples.pop_front();
        verify();
    }

    void clear() {
        actual.clear();
        samples.clear();
        verify();
    }
};

int main() {
    try {
        Check regression(2);
        regression.append(1);
        regression.append(2);
        regression.resize(4);
        regression.append(3); // Must average to 2, not 5/3.

        // Empty, partially filled, full and wrapped rings; grow and shrink.
        for (std::size_t initial = 1; initial <= 6; ++initial) {
            for (std::size_t count = 0; count <= 3 * initial; ++count) {
                for (std::size_t target = 0; target <= 8; ++target) {
                    Check check(initial);
                    for (std::size_t i = 0; i < count; ++i) check.append(i + 1);
                    check.resize(target);
                    for (int i = 0; i < 20; ++i) check.append(100 + i);
                    check.resize(3);
                    check.append(-7);
                    check.resize(7);
                    check.append(11);
                    check.clear();
                    check.resize(5);
                    check.append(42);
                }
            }
        }
        std::cout << "MovingAverager resize regression passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
