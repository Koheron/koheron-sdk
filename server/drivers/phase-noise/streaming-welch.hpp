#pragma once

#include "welch-spectrum.hpp"
#include <condition_variable>
#include <mutex>
#include <span>
#include <thread>
#include "stream-performance.hpp"

namespace phase_noise {

// One new, independently detrended Hann segment per half-window hop. The last
// three segment periodograms form a rolling Welch estimate. The paired mode
// keeps two independent real transforms, preserving a quiet channel and the
// signed complex cross density. All FFT and spectrum buffers are reused.
template<std::size_t FftSize>
class StreamingWelch {
    static_assert(FftSize >= 32 && FftSize <= 65536 && (FftSize & (FftSize - 1)) == 0);
    static constexpr std::size_t bins = FftSize / 2 + 1;
    struct AlignedFree { void operator()(float* p) const { pffft_aligned_free(p); } };
    using Buffer = std::unique_ptr<float, AlignedFree>;
    static Buffer buffer() { return Buffer{static_cast<float*>(pffft_aligned_malloc(FftSize * sizeof(float)))}; }
    struct Workspace {
        Buffer transformed = buffer(), scratch = buffer();
        RawPhaseTrend trend{};
        std::array<double, 3> timing{};
        bool preparation_calibrated = false, use_neon_preparation = false;
    };
    std::unique_ptr<PFFFT_Setup, decltype(&pffft_destroy_setup)> plan{
        pffft_new_setup(int(FftSize), PFFFT_REAL), pffft_destroy_setup};
    detail::NativeRealFftLayout<FftSize> layout{plan.get()};
    std::array<std::size_t, bins> frequency_bins{};
    bool native_output = false;
    std::array<Workspace, 2> work;
    const std::vector<float> window = scicpp::signal::windows::hann<float>(FftSize);
    double window_power = 0;
    std::vector<float> power = std::vector<float>(bins), averaged_power = std::vector<float>(bins);
    std::vector<float> native_power = std::vector<float>(bins);
    std::vector<std::complex<float>> cross = std::vector<std::complex<float>>(bins);
    std::vector<std::complex<float>> native_cross = std::vector<std::complex<float>>(bins);
    std::array<std::vector<float>, 3> history{
        std::vector<float>(bins), std::vector<float>(bins), std::vector<float>(bins)};
    std::size_t head = 0, filled = 0;
    uint64_t segments = 0;
    std::array<double, 5> timing{};

    // A single persistent helper handles the second channel. The acquisition
    // thread computes the first channel while the helper runs, then joins it.
    std::mutex mutex;
    std::condition_variable wake, done;
    bool pending = false, stopping = false;
    std::span<const int32_t> background_raw;
    double background_scale = 0;
    std::thread worker;

    void transform(std::span<const int32_t> raw, double scale, Workspace& ws) {
        assert(raw.size() == FftSize);
        auto start = StreamClock::now();
        ws.trend = fit_raw_phase(raw);
        ws.timing[0] = elapsed_ms(start);
        start = StreamClock::now();
#if defined(__ARM_NEON)
        if (!ws.preparation_calibrated) {
            double reference_ms = std::numeric_limits<double>::max(), neon_ms = reference_ms;
            // Warm both kernels and alternate their order. Only select NEON
            // with a clear measured benefit; small/noisy wins keep the double
            // reference. This one-time check uses the first real capture.
            for (int trial = 0; trial < 4; ++trial) {
                for (int pass = 0; pass < 2; ++pass) {
                    const bool neon = (trial + pass) % 2 == 0;
                    const auto trial_start = StreamClock::now();
                    if (neon) prepare_phase_window(raw, ws.trend, scale, window, ws.transformed.get());
                    else prepare_phase_window_reference(raw, ws.trend, scale, window, ws.transformed.get());
                    auto& duration = neon ? neon_ms : reference_ms;
                    duration = std::min(duration, elapsed_ms(trial_start));
                }
            }
            ws.use_neon_preparation = neon_ms < .9 * reference_ms;
            ws.preparation_calibrated = true;
        }
        if (ws.use_neon_preparation) prepare_phase_window(raw, ws.trend, scale, window, ws.transformed.get());
        else prepare_phase_window_reference(raw, ws.trend, scale, window, ws.transformed.get());
#else
        prepare_phase_window_reference(raw, ws.trend, scale, window, ws.transformed.get());
#endif
        ws.timing[1] = elapsed_ms(start);
        start = StreamClock::now();
        pffft_transform(plan.get(), ws.transformed.get(), ws.transformed.get(),
                                ws.scratch.get(), PFFFT_FORWARD);
        ws.timing[2] = elapsed_ms(start);
    }
    void worker_loop() {
        std::unique_lock lock(mutex);
        for (;;) {
            wake.wait(lock, [&] { return pending || stopping; });
            if (stopping) return;
            const auto raw = background_raw;
            const double scale = background_scale;
            lock.unlock();
            transform(raw, scale, work[1]);
            lock.lock();
            pending = false;
            done.notify_one();
        }
    }
#if defined(__ARM_NEON)
    static uint32x4_t safe_magnitudes(float32x4_t values, float minimum, float maximum) {
        const auto bits = vandq_u32(vreinterpretq_u32_f32(values), vdupq_n_u32(0x7fffffffu));
        const auto low = vreinterpretq_u32_f32(vdupq_n_f32(minimum));
        const auto high = vreinterpretq_u32_f32(vdupq_n_f32(maximum));
        return vorrq_u32(vceqq_u32(bits, vdupq_n_u32(0)),
            vandq_u32(vcgeq_u32(bits, low), vcleq_u32(bits, high)));
    }
    static bool all_lanes(uint32x4_t mask) {
        const auto halves = vreinterpretq_u64_u32(mask);
        return (vgetq_lane_u64(halves, 0) & vgetq_lane_u64(halves, 1)) == UINT64_MAX;
    }
#endif
    void fill_cross(float normalization) {
        const auto* x = work[0].transformed.get();
        const auto* y = work[1].transformed.get();
        const auto nyquist = layout.simd_size == 1 ? FftSize - 1 : 4;
        native_cross.front() = {x[0] * y[0] * normalization, 0};
        native_cross.back() = {x[nyquist] * y[nyquist] * normalization, 0};
        const float factor = 2.f * normalization;
        if (layout.simd_size == 1) {
            for (std::size_t k = 1; k < bins - 1; ++k) {
                const auto r = 2*k - 1, i = 2*k;
                native_cross[k] = {factor * (x[r] * y[r] + x[i] * y[i]),
                                   factor * (x[r] * y[i] - x[i] * y[r])};
            }
        } else {
            for (std::size_t k = 1; k < 4; ++k)
                native_cross[k] = {factor * (x[k] * y[k] + x[k+4] * y[k+4]),
                                   factor * (x[k] * y[k+4] - x[k+4] * y[k])};
#if defined(__ARM_NEON)
            // Preserve scalar VFP behavior for values that could become
            // subnormal during products, cancellation or normalization.
            const float minimum = std::sqrt(std::numeric_limits<float>::min() / factor) * 65536.f;
#endif
            for (std::size_t offset = 8; offset < FftSize; offset += 8) {
#if defined(__ARM_NEON)
                const auto ar = vld1q_f32(x + offset), ai = vld1q_f32(x + offset + 4);
                const auto br = vld1q_f32(y + offset), bi = vld1q_f32(y + offset + 4);
                auto safe = safe_magnitudes(ar, minimum, 0x1p60f);
                safe = vandq_u32(safe, safe_magnitudes(ai, minimum, 0x1p60f));
                safe = vandq_u32(safe, safe_magnitudes(br, minimum, 0x1p60f));
                safe = vandq_u32(safe, safe_magnitudes(bi, minimum, 0x1p60f));
                if (all_lanes(safe)) {
                    float32x4x2_t output;
                    output.val[0] = vmulq_n_f32(vaddq_f32(vmulq_f32(ar, br), vmulq_f32(ai, bi)), factor);
                    output.val[1] = vmulq_n_f32(vsubq_f32(vmulq_f32(ar, bi), vmulq_f32(ai, br)), factor);
                    vst2q_f32(reinterpret_cast<float*>(native_cross.data() + offset/2), output);
                    continue;
                }
#endif
                for (std::size_t lane = 0; lane < 4; ++lane) {
                    const auto r = offset + lane, i = r + 4;
                    native_cross[offset/2 + lane] = {factor * (x[r] * y[r] + x[i] * y[i]),
                                                    factor * (x[r] * y[i] - x[i] * y[r])};
                }
            }
        }
        // Publish only the compact CSD permutation, never reorder two complete
        // complex FFT buffers. Both channels share the same cached plan/layout.
        if (!native_output)
            for (std::size_t k = 0; k < bins; ++k) cross[k] = native_cross[layout.positions[k]];
    }
  public:
    StreamingWelch() {
        assert(plan);
        for (std::size_t k = 0; k < bins; ++k) frequency_bins[layout.positions[k]] = k;
        for (const auto value : window) window_power += double(value) * double(value);
    }
    ~StreamingWelch() {
        if (!worker.joinable()) return;
        { std::lock_guard lock(mutex); stopping = true; }
        wake.notify_one();
        worker.join();
    }
    StreamingWelch(const StreamingWelch&) = delete;
    StreamingWelch& operator=(const StreamingWelch&) = delete;
    void reset() { head = filled = 0; }
    void process(std::span<const int32_t> x, double scale_x, double fs,
                 std::span<const int32_t> y = {}, double scale_y = 0, bool rolling = true, bool native = false) {
        assert(fs > 0);
        if (native_output != native) { reset(); native_output = native; }
        if (!y.empty()) {
            if (!worker.joinable()) worker = std::thread([this] { worker_loop(); });
            { std::lock_guard lock(mutex); background_raw = y; background_scale = scale_y; pending = true; }
            wake.notify_one();
        }
        transform(x, scale_x, work[0]);
        double wait_ms = 0;
        if (!y.empty()) {
            const auto start = StreamClock::now();
            std::unique_lock lock(mutex);
            done.wait(lock, [&] { return !pending; });
            wait_ms = elapsed_ms(start);
        }
        for (std::size_t i = 0; i < 3; ++i)
            timing[i] = y.empty() ? work[0].timing[i] : std::max(work[0].timing[i], work[1].timing[i]);
        timing[4] = wait_ms;
        const auto reduce_start = StreamClock::now();
        const float normalization = float(1.0 / (fs * window_power));
        if (y.empty()) {
            std::fill(native_power.begin(), native_power.end(), 0.f);
            detail::accumulate_welch_power(work[0].transformed.get(), native_power.data(), FftSize, layout.simd_size);
        } else {
            fill_cross(normalization);
            if (!rolling) { timing[3] = elapsed_ms(reduce_start); ++segments; return; }
        }
        for (std::size_t k = 0; k < bins; ++k) {
            const float factor = normalization * (k == 0 || k == bins - 1 ? 1.f : 2.f);
            if (y.empty()) power[k] = native_power[native_output ? k : layout.positions[k]] * factor;
            else {
                power[k] = (native_output ? native_cross[k] : cross[k]).real();
            }
            history[head][k] = power[k];
        }
        head = (head + 1) % 3;
        filled = std::min(filled + 1, std::size_t{3});
        ++segments;
        const float reciprocal = 1.f / float(filled);
        std::size_t k = 0;
#if defined(__ARM_NEON)
        if (filled == 3) for (; k + 4 <= bins; k += 4) {
            const auto a = vld1q_f32(history[0].data() + k);
            const auto b = vld1q_f32(history[1].data() + k);
            const auto c = vld1q_f32(history[2].data() + k);
            auto safe = safe_magnitudes(a, 0x1p-94f, 0x1p100f);
            safe = vandq_u32(safe, safe_magnitudes(b, 0x1p-94f, 0x1p100f));
            safe = vandq_u32(safe, safe_magnitudes(c, 0x1p-94f, 0x1p100f));
            if (all_lanes(safe)) {
                vst1q_f32(averaged_power.data() + k, vmulq_n_f32(vaddq_f32(vaddq_f32(a, b), c), reciprocal));
            } else for (std::size_t j = k; j < k + 4; ++j)
                averaged_power[j] = ((history[0][j] + history[1][j]) + history[2][j]) * reciprocal;
        }
#endif
        for (; k < bins; ++k) {
            float sum = 0;
            for (std::size_t j = 0; j < filled; ++j) sum += history[j][k];
            averaged_power[k] = sum * reciprocal;
        }
        timing[3] = elapsed_ms(reduce_start);
    }
    const auto& latest_power() const { return power; }
    const auto& latest_cross() const { return native_output ? native_cross : cross; }
    const auto& density() const { return averaged_power; }
    auto trend(std::size_t channel = 0) const { return work[channel].trend; }
    uint64_t segment_count() const { return segments; }
    uint32_t retained_segments() const { return uint32_t(filled); }
    const auto& stage_times() const { return timing; }
    std::size_t frequency_bin(std::size_t k) const { return native_output ? frequency_bins[k] : k; }
    template<typename T>
    void order_to(const std::vector<T>& native, std::vector<T>& ordered) const {
        assert(native.size() == bins && &native != &ordered);
        ordered.resize(bins);
        for (std::size_t k = 0; k < bins; ++k) ordered[k] = native[native_output ? layout.positions[k] : k];
    }
};

} // namespace phase_noise
