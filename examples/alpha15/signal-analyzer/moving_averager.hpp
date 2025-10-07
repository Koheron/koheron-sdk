#ifndef __ALPHA15_SIGNAL_ANALYZER_MOVING_AVERAGER_HPP__
#define __ALPHA15_SIGNAL_ANALYZER_MOVING_AVERAGER_HPP__

#include <array>
#include <vector>
#include <scicpp/core.hpp>

template<std::size_t navg, typename T=double>
struct MovingAverager {
    void append(std::vector<T> &&vec) {
        // Assume all append vectors have the same size
        circ_buffer[idx] = std::move(vec);
        inc_idx();
    }

    void append(const std::vector<T> &vec) {
        circ_buffer[idx] = vec;
        inc_idx();
    }

    bool full() const {
        return is_full;
    }

    auto average() {
        using namespace scicpp::operators;

        auto res = scicpp::zeros<T>(circ_buffer[0].size());

        for (const auto& vec : circ_buffer) {
            res = std::move(res) + vec;
        }

        return res / T(navg);
    }

  private:
    std::size_t idx = 0;
    bool is_full = false;
    std::array<std::vector<T>, navg> circ_buffer{};

    void inc_idx() {
        if (idx == navg - 1) {
            is_full = true;
        }

        ++idx;
        idx %= navg;
    }
}; // MovingAverager

#endif // __ALPHA15_SIGNAL_ANALYZER_MOVING_AVERAGER_HPP__