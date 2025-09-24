#ifndef __ALPHA250_PHASE_NOISE_ANALYZER_MOVING_AVERAGER_HPP__
#define __ALPHA250_PHASE_NOISE_ANALYZER_MOVING_AVERAGER_HPP__

#include <vector>
#include <cstddef>
#include <cassert>
#include <algorithm>

template<typename T>
class MovingAverager {
  public:
    explicit MovingAverager(std::size_t navg)
    : navg_(navg), ring_(navg), head_(0), filled_(0), width_(0) {}

    void resize(std::size_t new_navg) {
        navg_ = new_navg;
        ring_.assign(navg_, {});      // reset ring (vectors empty)
        sum_.clear();                 // reset running sum
        width_ = 0;
        head_ = 0;
        filled_ = 0;
    }

    void append(const std::vector<T>& v) {
        ensure_shape(v.size());
        const std::size_t slot = next_slot();
        write_slot(slot, v);
    }

    void append(std::vector<T>&& v) {
        ensure_shape(v.size());
        const std::size_t slot = next_slot();
        write_slot(slot, std::move(v));
    }

    std::vector<T> average() const {
        if (filled_ == 0) {
            return {};
        }

        std::vector<T> out(sum_);
        const T denom = static_cast<T>(filled_);

        for (auto& x : out) {
            x /= denom;
        }

        return out;
    }

    void set_navg(std::size_t new_navg) {
        if (new_navg == 0) {
            new_navg = 1;
        }

        if (new_navg == navg_) {
            return;
        }

        // If we have no shape yet, just reset structures
        if (width_ == 0) {
            navg_ = new_navg;
            ring_.assign(navg_, {});
            head_ = 0;
            filled_ = 0;
            return;
        }

        if (new_navg > navg_) {
            // GROW: keep running sum & positions; just add capacity
            const auto old_n = navg_;
            navg_ = new_navg;
            ring_.resize(navg_);

            for (std::size_t i = old_n; i < navg_; ++i) {
                ring_[i].reserve(width_);
            }

            return;
        }

        // new_navg < navg_
        // SHRINK: keep the most recent `kept = min(filled_, new_navg)` samples
        const std::size_t kept = std::min(filled_, new_navg);
        const std::size_t drop = filled_ - kept;

        // Oldest sample index in current ring:
        // range of valid samples is [oldest ... oldest + filled_-1] circularly
        const std::size_t oldest = (head_ + navg_ - filled_) % navg_;

        // Subtract the dropped samples from the running sum
        for (std::size_t k = 0; k < drop; ++k) {
            const auto& old = ring_[(oldest + k) % navg_];
            if (old.size() == width_) {
                for (std::size_t i = 0; i < width_; ++i) {
                    sum_[i] -= old[i];
                }
            }
        }

        // Build a compact new ring of size new_navg with the kept (most-recent) samples
        std::vector<std::vector<T>> new_ring(new_navg);

        for (auto& v : new_ring) {
            v.reserve(width_);
        }

        // kept samples start at index (oldest + drop)
        for (std::size_t i = 0; i < kept; ++i) {
            new_ring[i] = std::move(ring_[(oldest + drop + i) % navg_]);
        }

        ring_  = std::move(new_ring);
        navg_  = new_navg;
        filled_ = kept;
        head_   = kept % navg_;  // next slot to overwrite
    }

    // Accessors
    std::size_t window() const noexcept { return navg_;  }
    std::size_t count()  const noexcept { return filled_; }
    bool full()          const noexcept { return filled_ == navg_; }
    std::size_t width()  const noexcept { return width_; }

  private:
    std::size_t navg_;
    std::vector<std::vector<T>> ring_;  // circular buffer
    std::vector<T> sum_;                // running sum
    std::size_t head_;                  // index to overwrite next
    std::size_t filled_;                // how many slots currently valid (≤ navg_)
    std::size_t width_;                 // element count per vector

    // Reserve/initialize on first append
    void ensure_shape(std::size_t w) {
        if (width_ == 0) {
            width_ = w;
            sum_.assign(width_, T(0));

            for (auto& v : ring_) {
                v.reserve(width_);
            }
        } else {
            assert(w == width_ && "All appended vectors must have identical length");
        }
    }

    std::size_t next_slot() {
        auto i = head_;
        head_ = (head_ + 1) % navg_;

        if (filled_ < navg_) {
            ++filled_;
        }

        return i;
    }

    template<typename Vec>
    void write_slot(std::size_t slot, Vec&& src) {
        // subtract old contribution (if present)
        if (ring_[slot].size() == width_) {
            const auto& old = ring_[slot];

            for (std::size_t i = 0; i < width_; ++i) {
                sum_[i] -= old[i];
            }
        }

        ring_[slot] = std::forward<Vec>(src);

        // add new contribution
        const auto& cur = ring_[slot];
        for (std::size_t i = 0; i < width_; ++i) {
            sum_[i] += cur[i];
        }
    }
};

#endif // __ALPHA250_PHASE_NOISE_ANALYZER_MOVING_AVERAGER_HPP__
