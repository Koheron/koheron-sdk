#ifndef __ALPHA250_PHASE_NOISE_ANALYZER_CUMULATIVE_AVERAGER_HPP__
#define __ALPHA250_PHASE_NOISE_ANALYZER_CUMULATIVE_AVERAGER_HPP__

#include <vector>
#include <cstddef>
#include <algorithm>

#include <scicpp/core.hpp>

template<typename T>
class CumulativeAverager {
  public:
    void clear() {
        sum_.clear();
        count_ = 0;
    }

    void append(const std::vector<T>& v) {
        using namespace scicpp::operators;

        if (sum_.empty()) {
            sum_.assign(v.size(), T{});
        }

        sum_ = std::move(sum_) + v;
        ++count_;
    }

    std::vector<T> average() const {
        using RepT = scicpp::units::representation_t<T>;
        using namespace scicpp::operators;

        if (count_ == 0) {
            return {};
        }

        return sum_ / static_cast<RepT>(count_);
    }

    std::size_t count() const noexcept {
        return count_;
    }

  private:
    std::vector<T> sum_;
    std::size_t count_ = 0;
};

#endif // __ALPHA250_PHASE_NOISE_ANALYZER_CUMULATIVE_AVERAGER_HPP__