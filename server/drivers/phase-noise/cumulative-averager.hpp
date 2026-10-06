#ifndef __ALPHA250_PHASE_NOISE_ANALYZER_CUMULATIVE_AVERAGER_HPP__
#define __ALPHA250_PHASE_NOISE_ANALYZER_CUMULATIVE_AVERAGER_HPP__

#include <vector>
#include <cstddef>
#include <algorithm>
#include <cassert>

#include <scicpp/core.hpp>

template<typename T>
class CumulativeAverager {
  public:
    void clear() {
        sum_.clear();
        count_ = 0;
    }

    void append(const std::vector<T>& v) {
        append_transformed(v, [](const T& value) { return value; });
    }

    template<typename Array, typename Convert>
    void append_transformed(const Array& values, Convert convert) {
        if (sum_.empty()) sum_.assign(values.size(), T{});
        assert(sum_.size() == values.size());
        for (std::size_t i = 0; i < values.size(); ++i) sum_[i] += convert(values[i]);
        ++count_;
    }

    template<typename Value>
    void average_real_to(std::vector<Value>& output) const {
        using RepT = scicpp::units::representation_t<Value>;
        output.resize(sum_.size());
        const RepT reciprocal = count_ ? RepT{1} / static_cast<RepT>(count_) : RepT{};
        for (std::size_t i = 0; i < output.size(); ++i)
            output[i] = sum_[i].real() * reciprocal;
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
