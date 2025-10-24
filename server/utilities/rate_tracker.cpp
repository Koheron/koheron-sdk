#include "server/utilities/rate_tracker.hpp"
#include "server/runtime/syslog.hpp"

#include <array>
#include <format>
#include <charconv>
#include <fstream>

namespace ut {

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
    window_bps_ = sum_last_seconds(window_sec_) * 8.0 / std::max(1, window_sec_);

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

// ------------------------------------------------
// Dump data rates to JSON

inline bool atomic_write(const std::filesystem::path& path, std::string_view data) {
    namespace fs = std::filesystem;

    // Ensure parent directory exists (if any)
    if (!path.parent_path().empty()) {
        std::error_code ec;
        fs::create_directories(path.parent_path(), ec);

        if (ec) {
            return false;
        }
    }

    // Build tmp path in the same directory: "<filename>.tmp"
    fs::path tmp = path;
    tmp += ".tmp";

    // Write file (binary, truncated)
    {
        std::ofstream ofs(tmp, std::ios::binary | std::ios::trunc);

        if (!ofs) {
            // couldn't open tmp file
            return false;
        }

        ofs.write(data.data(), static_cast<std::streamsize>(data.size()));

        if (!ofs) {
            ofs.close();
            std::error_code er;
            fs::remove(tmp, er); // best-effort cleanup
            return false;
        }

        ofs.flush();

        if (!ofs) {
            ofs.close();
            std::error_code er;
            fs::remove(tmp, er);
            return false;
        }
        // ofs closes on scope exit
    }

    std::error_code ec;
    fs::rename(tmp, path, ec);

    if (ec) {
        std::error_code er;
        fs::remove(tmp, er); // best-effort cleanup
        return false;
    }

    return true;
}

inline std::string json_escape(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 8); // small slack

    for (unsigned char c : s) {
        switch (c) {
            case '\"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20) {
                    // Emit \u00XX (4 hex digits, lower-case hex is fine for JSON)
                    char hexbuf[8]; // plenty for to_chars
                    auto res = std::to_chars(hexbuf, hexbuf + sizeof(hexbuf),
                                             static_cast<unsigned>(c), 16);
                    const int n = res.ptr - hexbuf;
                    out += "\\u";

                    for (int i = n; i < 4; ++i) {  // left-pad to width 4
                        out += '0';
                    }
    
                    out.append(hexbuf, res.ptr);
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    return out;
}

inline void append_u64(std::string& out, uint64_t v) {
    char buf[32];
    auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), v);
    out.append(buf, ptr);
}

inline void append_i64(std::string& out, int64_t v) {
    char buf[32];
    auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), v);
    out.append(buf, ptr);
}

inline void append_fixed(std::string& out, double v, int precision) {
    char buf[64];
    auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), v,
                                   std::chars_format::fixed, precision);
    out.append(buf, ptr);
}

inline std::string build_json(const std::vector<RateRow>& rows,
                              std::string_view collection_key) {
    using clock = std::chrono::steady_clock;
    using sec   = std::chrono::duration<double>;

    const auto now  = clock::now().time_since_epoch();
    const auto secs = std::chrono::duration_cast<sec>(now).count();

    std::string j;
    j.reserve(1024 + rows.size() * 512);

    j += "{\"ts\":";
    append_fixed(j, secs, 6);

    j += ",\"";
    j.append(collection_key);  // key name (caller-provided)
    j += "\":[";

    bool first = true;
    for (const auto& r : rows) {
        if (!first) {
            j += ',';
        }

        first = false;

        j += "{";

        // "id": <num>
        j += "\"id\":";
        append_i64(j, static_cast<int64_t>(r.id));

        // "name": "<escaped>"
        j += ",\"name\":\"";
        j += json_escape(r.name);
        j += "\"";

        // "rx_total": <num>
        j += ",\"rx_total\":";
        append_i64(j, r.total_rx);

        // "tx_total": <num>
        j += ",\"tx_total\":";
        append_i64(j, r.total_tx);

        // doubles with 3 decimals
        auto add_d = [&](const char* k, double v){
            j += ",\"";
            j += k;
            j += "\":";
            append_fixed(j, v, 3);
        };

        add_d("rx_mean", r.rx_mean);
        add_d("rx_win",  r.rx_win);
        add_d("rx_inst", r.rx_inst);
        add_d("rx_ewma", r.rx_ewma);
        add_d("rx_max",  r.rx_max);
        add_d("tx_mean", r.tx_mean);
        add_d("tx_win",  r.tx_win);
        add_d("tx_inst", r.tx_inst);
        add_d("tx_ewma", r.tx_ewma);
        add_d("tx_max",  r.tx_max);

        j += "}";
    }

    j += "]}";
    return j;
}

bool dump_rates_to_json(const std::filesystem::path& path,
                        std::string_view collection_key,
                        const std::vector<RateRow>& rows) {
    return atomic_write(path, build_json(rows, collection_key));
}

} // namespace ut