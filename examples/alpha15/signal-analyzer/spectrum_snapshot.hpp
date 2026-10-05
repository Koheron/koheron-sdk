#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>
#include <tuple>
#include <vector>

// The acquisition mutex protects both publication and restart. Sequence zero
// means that no complete, freshly averaged frame exists in this generation.
class SpectrumSnapshot {
    uint32_t generation = 0;
    uint64_t sequence = 0;
    std::vector<float> values;

  public:
    explicit SpectrumSnapshot(std::size_t bins) : values(bins) { restart(); }

    uint32_t restart() {
        ++generation;
        sequence = 0;
        std::fill(values.begin(), values.end(), std::numeric_limits<float>::quiet_NaN());
        return generation;
    }

    template<class Values>
    void publish(const Values& next) {
        values.assign(next.begin(), next.end());
        ++sequence;
    }

    auto snapshot() const { return std::tuple{generation, sequence, values}; }
};
