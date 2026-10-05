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

All three designs use cyclic scatter/gather DMA. ALPHA250-4 retains its paired
X/Y ring; ALPHA250 and Red Pitaya share `cyclic-phase-dma.hpp`, a single-stream
ring of 512 packets of 8192 int32 samples (16 MiB payload plus descriptors).
The FPGA continues filling DDR while software computes FFTs or serves clients.
Readers select the latest complete window, require 65536 new samples for the
single-channel Welch estimator, validate descriptor status and queued precision/
overrange/gap metadata, then recheck ring retention after the copy. Falling
behind discards older windows, without changing sample spacing within a window.
Two packets are withheld for DDR writeback and the first packet of each epoch is
excluded for filter settling. Watchdogs reject stopped producers, malformed
completed descriptors and ambiguous 24-bit packet-sequence rollover.

Rate, channel, precision and manual LO changes stop the stream, reset phase,
CIC, FIR and FIFO histories, then restart an acquisition epoch with DMA armed.
A generation check rejects a window copied before a concurrent change. Gap or
overrange metadata is sticky until reset; affected captures clear averages and
trigger automatic restart. Shutdown cancels a waiting read without waiting for
a full slow-decimation window. The FPGA phase accumulator is 64 bits; a range
guard and packet quantizer still bound the calibrated 32-bit DMA output.

Tracking retunes also establish a new hardware epoch to prevent a window from
straddling a DDS update. Acquisition is continuous between retunes; intentional
setting changes and recovery discard data. This does not promise an exhaustive
FFT of every sample or 60 fresh spectra/s. FFT sizes and board calibration are
unchanged by the DMA port.

ALPHA250 and Red Pitaya append `get_dma_status()` without changing older RPC
shapes. It returns four uint64 values: completed packet count, consumed packet
count, acquisition generation, and sample-gap capture count. Packet counters
stay monotonic across restarts. Existing precision status reports state 4 for
a discarded sample gap, alongside overflow/DMA-error counts.

## Validation

Run both ALPHA PNA software runners and the ALPHA250-4 RTL runner. The shared
publication regression tests concurrent metadata/data reads and emits a frame
for the browser's production decoder. Python regressions exercise both
single-channel board adapters. FPGA block-design and routed timing checks are
separate from hardware PM and throughput measurements.
