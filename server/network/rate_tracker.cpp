#include "server/network/rate_tracker.hpp"
#include "server/runtime/syslog.hpp"

#include <format>

namespace net {

void RateTracker::update(int64_t bytes) {
    if (bytes < 0) {
        return;
    }

    total_bytes_ += bytes;

    // 1) bucketize per-second to maintain rolling window
    const auto now = clock::now();
    const auto s = sec(now);
    advance_buckets(s);
    buf_[idx_] += bytes;

    // 2) instantaneous 1s rate (sum last 1s bucket)
    inst_bps_ = buf_[idx_] * 8.0; // bits per second (per-current second)

    // 3) rolling window rate
    window_bps_ = sum_last_seconds(window_sec_) * 8.0;

    // 4) EWMA (event-driven, not time-stepped)
    // Use per-second rate of this second as sample for EWMA.
    const double sample_bps = inst_bps_;
    ewma_bps_ = ewma_alpha_ * sample_bps + (1.0 - ewma_alpha_) * ewma_bps_;

    // 5) max window rate
    max_bps_ = std::max(max_bps_, window_bps_);

    last_ = now;
}

RateTracker::Snapshot RateTracker::snapshot() const {
    const auto now = clock::now();
    const double dt = std::chrono::duration<double>(now - start_).count();
    const double mean = dt > 0 ? (total_bytes_ * 8.0) / dt : 0.0;
    return Snapshot{
        .mean_bps        = mean,
        .window_bps      = window_bps_,
        .inst_bps        = inst_bps_,
        .ewma_bps        = ewma_bps_,
        .max_bps         = max_bps_,
        .total_bytes     = total_bytes_,
        .seconds_elapsed = dt
    };
}

inline void format_rate(char* out, std::size_t n, double bps) {
    if (!n) return; // nothing to write

    double v = bps;
    std::string_view unit = "bps";
    if (v >= 1e12) { v /= 1e12; unit = "Tbps"; }
    else if (v >= 1e9)  { v /= 1e9;  unit = "Gbps"; }
    else if (v >= 1e6)  { v /= 1e6;  unit = "Mbps"; }
    else if (v >= 1e3)  { v /= 1e3;  unit = "Kbps"; }

    // Write at most n-1 chars and terminate.
    auto res = std::format_to_n(out, n - 1, "{:.3f} {}", v, unit);
    out[std::min<std::size_t>(res.size, n - 1)] = '\0';
}

void RateTracker::log_snapshot(std::string_view label) const {
    const auto s = snapshot();

    char mean_h[32]{}, win_h[32]{}, inst_h[32]{}, ewma_h[32]{}, max_h[32]{};
    format_rate(mean_h, sizeof(mean_h), s.mean_bps);
    format_rate(win_h,  sizeof(win_h),  s.window_bps);
    format_rate(inst_h, sizeof(inst_h), s.inst_bps);
    format_rate(ewma_h, sizeof(ewma_h), s.ewma_bps);
    format_rate(max_h,  sizeof(max_h),  s.max_bps);

    logf(
        "[{}] mean={}  window={}  inst={}  ewma={}  max={}  total={} B  elapsed={:.3f} s\n",
        label,
        mean_h, win_h, inst_h, ewma_h, max_h,
        s.total_bytes, s.seconds_elapsed
    );
}

void RateTracker::advance_buckets(uint64_t now_s) {
    if (now_s == bucket_ts_) return;
    // move forward one or more seconds; zero intermediate buckets
    auto delta = std::min<uint64_t>(now_s - bucket_ts_, buckets_);
    for (uint64_t i = 0; i < delta; ++i) {
        idx_ = (idx_ + 1) % buckets_;
        buf_[idx_] = 0;
    }
    bucket_ts_ = now_s;
}

uint64_t RateTracker::sum_last_seconds(int n) const {
    n = std::min(n, buckets_);
    uint64_t sum = 0;
    for (int i = 0; i < n; ++i) {
        int j = idx_ - i;
        if (j < 0) j += buckets_;
        sum += buf_[j];
    }
    return sum;
}

} // namespace net