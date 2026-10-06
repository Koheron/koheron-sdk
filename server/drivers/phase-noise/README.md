# Shared PNA processing

ALPHA250, ALPHA250-4 and Red Pitaya use `streaming-welch.hpp`: a 32768-point
symmetric Hann FFT advanced by 16384 samples. Each segment is detrended in raw
integer counts before conversion to radians. Auto spectra publish the mean of
the latest three segment periodograms, with the configured moving average over
those rolling estimates. Segment FFTs are computed once and reused. Successive
published estimates are correlated; their average count is not a count of
independent observations.

ALPHA250-4 XY uses two synchronized real FFTs and the signed complex cross
density. Each new segment enters the cumulative average exactly once, rather
than appending overlapping three-segment estimates repeatedly. Its count is the
number of processed segments, including their 50% overlap, not an independent
sample count. The paired helper thread persists across segments. FFT plans,
weighted inputs, scratch buffers and rolling periodograms are reused. Phase
snapshots are converted on request, outside the recurring FFT preparation.

The engine incorporates [PR #772](https://github.com/Koheron/koheron-sdk/pull/772)
and its stacked [PR #773](https://github.com/Koheron/koheron-sdk/pull/773).
Both channels transform in native FFT order; the cached, plan-derived layout
maps compact power/CSD values into published bins. No complete complex FFT
buffer is reordered on each hop. The batch reference uses the same layout.
FFT input/output share an aligned buffer, reducing the paired workspace size.
ARM integer trend fitting is exact; SIMD spectrum arithmetic retains scalar
fallbacks for values whose subnormal behavior ARMv7 NEON cannot preserve.

`phase-ring.hpp` supplies one shared cyclic SG DMA reader for one or two streams.
Readers process successive half-window hops in chronological order, rather than
selecting the latest window and silently skipping queued hops. A window lost to
ring overwrite increments an overrun counter and resumes at the newest complete
window on the same hop grid. The returned `skipped_hops` reports missing windows;
no FFT joins samples across the gap. Retained three-segment Welch history resets,
while valid moving/cumulative averages and the hardware epoch are preserved.
The packet precision, gap, descriptor and DDR-writeback checks remain required.
Board calibration and synchronized FPGA sample admission remain explicit.
The reader caches retained overlap and copies only new payload chunks from
DDR; metadata and descriptor checks still cover the complete window. Watchdog
or actual DMA-error recovery rearms a requested acquisition even after its
running flag has been cleared by the error. Consumer overruns do not restart DMA.
The browser reports skipped coverage in its precision status and tooltip.

`get_stream_status()` returns `(processed_segments, ring_overruns, fft_size,
hop_size, welch_depth)` with wire format `QQIII`. Segment count is monotonic;
retained Welch history is cleared on settings/epoch changes. Welch starts with
available segments and reaches depth three during warmup. Single-channel phase
snapshots retain their 65536-sample RPC shape; quad snapshots remain 32768 samples.
All spectra now contain 16385 bins, spaced by `fs / 32768`.

The browser uses `web/phase-noise/plot.ts` and captured metadata for axes/exports.
Single-channel Python clients share `koheron.phase_noise.SingleChannelPhaseNoiseAnalyzer`.
Install the SDK Python package when updating clients. The old batch Welch and
multirate helpers remain numerical regression references; production analyzers
use the shared streaming estimator.

## Sample coverage

`get_stream_coverage()` returns `(epoch, covered_chunks, span_chunks)` as `QQQ`.
Each chunk represents 8192 phase samples after decimation. The counters measure
the union of accepted FFT windows from the first valid window through the latest
accepted window; overlap counts once. Queued samples are excluded until accepted
or skipped. A missed FFT hop can still leave 100% sample coverage when neighboring
windows cover the complete interval. Coverage measures sample-time gaps, not the
number of FFT segments or independent averages. Single-channel seed windows cover
65536 samples; steady-state FFTs cover only their final 32768 samples.

Settings changes, invalid acquisitions and manual XY resets start a new coverage
epoch. Consumer overruns preserve it and add only the recent window's actual
sample coverage. Chrome shows `Coverage 100%` beside FPS and connection status when recent acquisitions have no
uncovered sample interval, using approximately ten seconds of counter updates.
Its tooltip reports total coverage/skipped sample time since acquisition reset.
FPGA sample-loss metadata is reported separately because its missing-sample count
is not available. Older instruments show `Coverage n/a`; disconnected or settling
instruments show an unavailable value rather than stale coverage.

## Atomic spectrum snapshot

All three designs expose `get_spectrum_snapshot()`. The result is a tuple:

```
(sequence, state, precision, fs, channel, cic_rate, fft_navg,
 average_count, average_target, lo0, lo1, lo2, lo3, mode, delay,
 reference_clock, density)
```

Sequence is a uint64 publication counter. State 1 means valid; other states
mark settling, overrange, DMA failure or a sample gap. A changed
publication, including invalidation, advances sequence. Average target zero
means cumulative XY averaging. Unused LOs are zero; mode/delay are zero for
ALPHA250-4. LO fields describe applied frequencies; saved nominal frequencies
remain available from the existing tracking/nominal RPCs. Density is the full
signed rad²/Hz vector, including DC and Nyquist.

Scalar metadata is 92 network-endian bytes (`QIIdIIIIIddddIdI`), followed by a
uint32 network-endian vector byte count and native little-endian float samples.
A single regular RPC header precedes the tuple. The browser validates framing,
uses captured settings for axes/references/exports and counts new sequence
numbers for FPS. Older instruments fall back to the existing spectrum RPC;
only that fallback has separately polled settings.

`SpectrumPublication` owns its data and metadata under one short lock. Writers
hold the analyzer processing lock before publishing; readers acquire only the
publication lock and do not wait for acquisition or FFT work.

## Acquisition boundaries

All three designs use cyclic scatter/gather DMA with 8192-sample packets and
512 retained sample windows. The single-stream ring contains 16 MiB payload;
the paired ring contains 32 MiB. FPGA acquisition continues while Linux computes
FFTs or serves clients. Reads retain half-window overlap, validate descriptor
status and queued precision/overrange/gap metadata, then recheck ring retention.
Two physical packets are withheld for DDR writeback, and the first sample chunk
of an epoch is excluded for settling. Watchdogs reject stopped producers,
malformed completed descriptors and ambiguous 24-bit packet-sequence rollover.

Rate, channel, precision and manual LO changes stop the stream, reset phase,
CIC, FIR and FIFO histories, then restart an acquisition epoch with DMA armed.
Reapplying an unchanged channel, rate, precision or average target preserves
the running measurement. An LO request that rounds to the existing nominal
and applied tuning word also preserves it. Repeating the nominal LO still
retunes when tracking has moved the applied frequency.
A generation check rejects a window copied before a concurrent change. Gap or
overrange metadata is sticky until reset; affected captures clear averages and
trigger automatic restart. Shutdown cancels a waiting read without waiting for
a full slow-decimation window. The FPGA phase accumulator is 64 bits; a range
guard and packet quantizer still bound the calibrated 32-bit DMA output.

Single-channel tracking retunes establish a new hardware epoch to prevent a window
from straddling a DDS update. The quad design retains its continuous, slow DDS
tracking updates. Acquisition is continuous between resets; intentional
setting changes and recovery discard data. This does not promise an exhaustive
FFT of every sample or 60 fresh spectra/s. Board calibration is unchanged by the streaming port.

ALPHA250 and Red Pitaya append `get_dma_status()` without changing older RPC
shapes. It returns four uint64 values: completed packet count, consumed packet
count, acquisition generation, and sample-gap capture count. Packet counters
stay monotonic across restarts. Existing precision status reports state 4 for
a discarded sample gap, alongside overflow/DMA-error counts.

## Validation

Run both ALPHA PNA software runners and the ALPHA250-4 RTL runner.
`tests/test_streaming_welch.cpp` exercises the persistent worker and half-window
scheduler; `tests/check_streaming_welch.py` independently compares segment auto
and signed cross spectra, rolling means, large drift and a 120 dB channel-power
ratio against SciPy. Both DMA simulators check exact retained overlap. The
three-segment history changes the close-offset detrending response; modulation,
bias and convergence measurements must use the new estimator. The shared
publication regression tests concurrent metadata/data reads and emits a frame
for the browser's production decoder. Python regressions exercise both
single-channel board adapters. FPGA block-design and routed timing checks are
separate from hardware PM and throughput measurements.

## Processing capacity and display cadence

All three designs accumulate every accepted half-window FFT. Displayed spectra,
metadata and integrated jitter are materialized at up to 30 Hz; settings changes,
invalidations and the first valid result publish immediately. Averaging retains
native FFT-bin order between publications. Only the final output is permuted into
ascending frequency order, including the interferometer compensation's matching
frequency-bin lookup. Atomic snapshot counts describe the displayed spectrum.

Moving-average ring slots and publication vectors retain their allocations.
The shared DMA cache retains only the tail needed by the next overlapping window.
ARM builds use NEON. Each FFT workspace times both window-preparation kernels
on its first capture and selects NEON only when it is at least 10% faster than
the double reference. The window-preparation fast path
subtracts the raw fitted ramp using signed Q31.32 arithmetic before converting
its residual to float. Intercept/slope quantization contributes less than 7.7e-6
raw counts for windows of up to 65536 samples, before normal float rounding.
Out-of-range fitted lines/residuals and extreme scales use the double reference
path. The integer least-squares fit, Hann weights and spectral normalization are
retained. Scalar builds use the double path.

`get_stream_performance()` returns nine doubles: smoothed total service, FFT,
average accumulation, spectrum/jitter publication and DMA-copy times in ms,
current queued sample time in ms, approximate retained-buffer time in ms,
required windows/s and estimated processing capacity in windows/s. Total service
includes DMA copying and processing through publication, and excludes waiting for
new samples. The capacity estimate is indicative; contention and scheduling can
reduce achieved throughput. Processing times in `get_precision_status()` now
include jitter and publication on every board, excluding DMA copying.

`get_fft_performance()` returns five doubles: smoothed fit, window preparation,
transform, density reduction and paired-worker wait times in ms. Paired transform
stages report the larger per-channel time; their sum is not a complete wall-clock
profile. Single-channel reseeding processes three FFTs, while the substage values
describe the last of those segments.

The shared header shows queued sample time beside coverage. Amber warns when
estimated capacity is below the required half-window rate or the queue exceeds
half its retained buffer. The tooltip exposes capacity and stage timings, so a
queue building behind an initial 100% coverage reading is visible before loss.
