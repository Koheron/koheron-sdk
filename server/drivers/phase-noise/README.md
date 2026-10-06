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
