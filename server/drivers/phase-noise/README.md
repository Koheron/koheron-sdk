# Shared PNA processing

ALPHA250 and Red Pitaya select `Core<Board>` and its 32768-point, 50%-overlapped
Welch estimator. ALPHA250-4 selects `multirate-spectrum.hpp`: 30000, 3000 and
300-point Hann transforms at fs, fs/10 and fs/100, with signed cross spectra.
Both estimators use cached PFFFT workspaces. Moving averaging, raw-count drift
fitting, CIC gain correction, tracking lock detection and spectrum publication
are shared. Estimator selection and board calibration remain explicit.

The browser workspace uses `web/phase-noise/plot.ts` with board adapters for
LO selection, reference labels and signed-density display. Single-channel
Python clients inherit `koheron.phase_noise.SingleChannelPhaseNoiseAnalyzer`.
Install the SDK Python package when updating the example clients.

## Atomic spectrum snapshot

All three designs expose `get_spectrum_snapshot()`. The result is a tuple:

```
(sequence, state, precision, fs, channel, cic_rate, fft_navg,
 average_count, average_target, lo0, lo1, lo2, lo3, mode, delay,
 reference_clock, density)
```

Sequence is a uint64 publication counter. State 1 means valid; other states
mark settling, overrange, DMA failure or an ALPHA250-4 sample gap. A changed
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

ALPHA250-4 uses continuous paired cyclic SG DMA with per-packet precision,
overrange and sample-gap metadata. ALPHA250 and Red Pitaya retain simple DMA
and rearm between acquisitions. Sharing DSP and publication does not establish
continuous acquisition on those boards. A future single-stream ring port must
cover FIFO history reset, accumulator headroom, packet metadata, cancellation,
retuning and slow tracking before hardware deployment.

## Validation

Run both ALPHA PNA software runners and the ALPHA250-4 RTL runner. The shared
publication regression tests concurrent metadata/data reads and emits a frame
for the browser's production decoder. Python regressions exercise both
single-channel board adapters. FPGA block-design and routed timing checks are
separate from hardware PM and throughput measurements.
