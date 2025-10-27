#ifndef __SERVER_UTILITIES_RATE_TRACKER__
#define __SERVER_UTILITIES_RATE_TRACKER__

#include <array>
#include <chrono>
#include <cstdint>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string_view>
#include <vector>

namespace ut {

/* RateTracker metrics (all rates are in bits per second unless noted)

    mean    : Long-term average since start.
                mean_bps = (total_bytes * 8) / seconds_elapsed;

    window  : Recent average over a sliding time window (e.g., last W seconds).
                Computed as sum(bytes in the last W 1-second buckets) * 8 / W.

    inst    : Instantaneous (current-second) rate.
                inst_bps = (bytes_in_current_1s_bucket) * 8;

    ewma    : Exponentially Weighted Moving Average of the instantaneous rate.
                ewma_bps = α * inst_bps + (1 - α) * ewma_bps_prev;
                α is derived from a chosen half-life (more α → more responsive).

    max     : Peak recent rate seen so far.
                Tracked as the maximum observed window_bps.

    total   : Total payload bytes observed since start.

    elapsed : Seconds since start of measurement.
*/

class RateTracker {
  public:
    using clock = std::chrono::steady_clock;

    // window_sec: rolling window for "current" rate (e.g. 5s or 10s)
    // buckets: number of 1-second buckets to keep (must be >= window_sec)
    explicit RateTracker(int window_sec = 5, int buckets = 64, double ewma_halflife_sec = 2.0)
    : window_sec_(window_sec), buckets_(buckets),
      ewma_alpha_(halflife_to_alpha(ewma_halflife_sec)),
      start_(clock::now()), last_(start_), bucket_ts_(sec(start_))
    {
        buf_.fill(0);
    }

    // record N new bytes at time now (default: now)
    void update(int64_t bytes);

    // Adds `bytes` spread uniformly over the last `dur_s` seconds.
    // This keeps mean/total exact and makes window/inst reasonable.
    template <typename Duration>
    void update_over_duration(int64_t bytes, Duration dur) {
        using seconds_d = std::chrono::duration<double>;
        double dur_s = std::chrono::duration_cast<seconds_d>(dur).count();
        if (bytes <= 0 || dur_s <= 0.0)
            return;

        const auto now_s = sec(clock::now());
        advance_buckets(now_s);

        int secs = std::min<int>(static_cast<int>(std::ceil(dur_s)), buckets_);
        uint64_t per_sec = static_cast<uint64_t>(bytes / std::max(1, secs));
        uint64_t remainder = bytes - per_sec * secs;

        for (int i = 0; i < secs; ++i) {
            int j = idx_ - i;
            if (j < 0)
                j += buckets_;
            buf_[j] += per_sec + (i == 0 ? remainder : 0);
        }
        total_bytes_ += bytes;

        inst_bps_   = buf_[idx_] * 8.0;
        window_bps_ = sum_last_seconds(window_sec_) * 8.0 / std::max(1, window_sec_);
        ewma_bps_   = ewma_alpha_ * inst_bps_ + (1.0 - ewma_alpha_) * ewma_bps_;
        max_bps_    = std::max(max_bps_, window_bps_);
    }

    struct Snapshot {
        double mean_bps;     // average since start
        double window_bps;   // rolling window rate (last window_sec seconds)
        double inst_bps;     // current second (coarse)
        double ewma_bps;     // smoothed rate
        double max_bps;      // max observed window_bps
        int64_t total_bytes;
        double seconds_elapsed;
    };

    Snapshot snapshot();

    void log_snapshot(std::string_view label = "rate");

    void reset() {
        const auto now = clock::now();
        start_ = last_ = now;
        bucket_ts_ = sec(now);
        buf_.fill(0);
        idx_ = 0;
        total_bytes_ = 0;
        inst_bps_ = window_bps_ = ewma_bps_ = max_bps_ = 0.0;
    }

  private:
    // convert half-life (seconds) to smoothing alpha
    static double halflife_to_alpha(double halflife_sec) {
        if (halflife_sec <= 0) return 1.0;
        // alpha = 1 - 0.5^(Δt/hl); with Δt≈1s sample cadence
        return 1.0 - std::exp2(-1.0 / halflife_sec);
    }

    static uint64_t sec(clock::time_point t) {
        return std::chrono::duration_cast<std::chrono::seconds>(t.time_since_epoch()).count();
    }

    void advance_buckets(std::uint64_t now_s);
    uint64_t sum_last_seconds(int n) const;
    void refresh_to_now();

    // config
    int   window_sec_;
    int   buckets_;
    double ewma_alpha_;

    // clocks & totals
    clock::time_point start_, last_;
    int64_t total_bytes_ = 0;

    // circular buffer of per-second byte counts
    std::array<uint64_t, 256> buf_{}; // max 256s; we use 'buckets_' subset
    int idx_ = 0;                     // current second index
    uint64_t bucket_ts_;              // epoch seconds of current bucket

    // derived metrics (bits/s)
    double inst_bps_   = 0.0;
    double window_bps_ = 0.0;
    double ewma_bps_   = 0.0;
    double max_bps_    = 0.0;
};

// ------------------------------------------------
// Dump data rates to JSON

struct RateRow {
    int id;
    std::string_view name;
    int64_t total_rx;
    int64_t total_tx;
    // Rate metrics are expressed in bits per second to match RateTracker::Snapshot.
    double rx_mean, rx_win, rx_inst, rx_ewma, rx_max;
    double tx_mean, tx_win, tx_inst, tx_ewma, tx_max;
};

bool dump_rates_to_json(const std::filesystem::path& path,
                        std::string_view collection_key,
                        const std::vector<RateRow>& rows);

} // namespace ut

#endif // __SERVER_UTILITIES_RATE_TRACKER__