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
    : navg_(navg), ring_(navg) {}

    // --- resets the ring to a new window and forgets shape (width_=0)
    void resize(std::size_t new_navg) {
        navg_ = (new_navg == 0) ? 1 : new_navg;
        ring_.assign(navg_, {});   // empty slots
        sum_.clear();
        width_ = 0;
        head_ = 0;
        filled_ = 0;
    }

    // --- soft clear: keep current width & capacity, drop samples & sum
    void clear() {
        if (width_ == 0) {
            head_ = 0;
            filled_ = 0;
            return;
        }

        std::fill(sum_.begin(), sum_.end(), T{});

        for (auto& v : ring_) {
            v.clear();   // keep capacity
        }

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
        new_navg = (new_navg == 0) ? 1 : new_navg;
        if (new_navg == navg_) {
            return;
        }

        if (width_ == 0) {
            // shapeless: just rebuild ring for the new window
            navg_ = new_navg;
            ring_.assign(navg_, {});
            head_ = 0;
            filled_ = 0;
            return;
        }

        if (new_navg > navg_) {
            // GROW: keep state; extend ring with empty, reserved slots
            const auto old_n = navg_;
            navg_ = new_navg;
            ring_.resize(navg_);

            for (std::size_t i = old_n; i < navg_; ++i) {
                ring_[i].reserve(width_);
            }
            // sum_, head_, filled_ remain valid
            return;
        }

        // SHRINK: keep most recent 'kept' samples, then rebuild sum
        const std::size_t kept = std::min(filled_, new_navg);
        const std::size_t oldest = (head_ + navg_ - filled_) % navg_;

        std::vector<std::vector<T>> new_ring(new_navg);

        for (auto& v : new_ring) {
            v.reserve(width_);
        }

        // copy/move most recent 'kept' samples into compact ring
        const std::size_t start = (oldest + (filled_ - kept)) % navg_;

        for (std::size_t i = 0; i < kept; ++i) {
            new_ring[i] = std::move(ring_[(start + i) % navg_]);
        }

        ring_   = std::move(new_ring);
        navg_   = new_navg;
        filled_ = kept;
        head_   = kept % navg_;
        rebuild_sum();  // ensures sum_ is exactly consistent
    }

    void clear_to_width(std::size_t width) {
        width_ = width;
        sum_.assign(width_, T{});

        for (auto& v : ring_) {
            v.clear();
            v.reserve(width_);
        }

        head_ = 0;
        filled_ = 0;
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
    std::size_t head_   = 0;            // index to overwrite next
    std::size_t filled_ = 0;            // how many slots currently valid (≤ navg_)
    std::size_t width_  = 0;            // element count per vector

    void ensure_shape(std::size_t w) {
        if (width_ == 0) {
            clear_to_width(w);
        } else {
            assert(w == width_ && "All appended vectors must have identical length");
        }
    }

    std::size_t next_slot() {
        const auto i = head_;
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
        // store new
        ring_[slot] = std::forward<Vec>(src);
        // add new contribution
        const auto& cur = ring_[slot];
        for (std::size_t i = 0; i < width_; ++i) {
            sum_[i] += cur[i];
        }
    }

    void rebuild_sum() {
        if (width_ == 0) {
            sum_.clear();
            return;
        }

        sum_.assign(width_, T{});
        const std::size_t oldest = (head_ + navg_ - filled_) % navg_;

        for (std::size_t k = 0; k < filled_; ++k) {
            const auto& v = ring_[(oldest + k) % navg_];

            if (v.size() != width_) {
                continue;
            }

            for (std::size_t i = 0; i < width_; ++i) {
                sum_[i] += v[i];
            }
        }
    }
};

#endif // __ALPHA250_PHASE_NOISE_ANALYZER_MOVING_AVERAGER_HPP__
