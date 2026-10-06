## Streaming Welch — version 1.3.0

This revision shares the streaming Welch engine and cyclic DMA reader with
ALPHA250 and Red Pitaya. A 32768-point real FFT starts every 16384 samples;
software processes every queued hop until ring capacity is exhausted. X/Y
publish a rolling three-segment Hann estimate, followed by the selected moving
average. XY cumulatively averages signed complex segment cross spectra, counting
each new segment once. Overlapping segments remain statistically correlated.

Software multirate FIR filtering and frequency-bin stitching are removed.
The spectrum has **16385 bins** with spacing **fs/32768**, replacing 15001 bins
at fs/30000. Per-segment integer-domain detrending replaces the former global
fit; close-offset response and averaging statistics change. Phase snapshots
remain 32768 samples per channel and the FPGA packet format is unchanged.

FFT workspaces and the paired worker persist across updates. Raw phase snapshots
are converted only when requested; settings locks are released during FFT work.
`get_stream_status()` reports processed segments, ring overruns, FFT length,
hop length and Welch depth. Epoch changes invalidate retained segment history.
An overwritten or damaged window is rejected rather than joined across a gap.
Consumer overruns resume at a complete recent window, preserving valid averages
and the DMA epoch. Only the three-segment Welch history resets across the skip;
Chrome reports skipped coverage in the acquisition status. Settings changes,
overflow, FPGA sample gaps and actual DMA errors still invalidate averages.

At CIC 133 the acquisition ceiling is approximately 45.9 segments/s; at CIC 50
it is approximately 122.1/s. Actual rates depend on processing and board load.
Build/software checks and board measurements are reported separately below.

On 192.168.1.12, XY at CIC 133 increased from 23.0 to 45.9 new segments/s,
with median processing falling from 36.0 to approximately 11 ms. Thirty-second
checks at CIC 100 and 133 reached 61.0 and 45.9/s without overruns, DMA errors,
sample gaps or overflows. CIC 50 exceeded sustained CPU capacity and explicitly
reported ring overruns. Use CIC 100 or higher for continuous half-window
coverage under the tested load. See [hardware validation](tests/hardware-validation.md#streaming-welch-commonization-2026-10-06-pna-130).

This instrument measures the phase difference between IN0/IN1 (X) or IN2/IN3
(Y), or their cross-spectrum (XY). Channel selectors are X = 0, Y = 1, XY = 2.
The server returns a one-sided phase PSD in rad²/Hz. Positive estimates convert
to single-sideband phase noise with `10 * log10(PSD / 2)`; frequency-noise
density is `f² * PSD` in Hz²/Hz.

The analyzer uses the [shared PNA plot and atomic spectrum snapshot](../../../server/drivers/phase-noise/README.md).
Captured settings accompany each spectrum; FPS uses its publication sequence.
Existing spectrum and phase RPCs remain available, and the browser supports
older firmware through the existing read path.

## Runtime phase precision

The acquisition toolbar selects **Standard** or **+1…+8 bits**. The CIC and
compensation FIR retain 40 bits; the packet quantizer rounds to even and
saturates into the 32-bit DMA output. Each extra bit halves radians per count
and the available phase range. Standard retains the previous nominal scale;
+8 gives 256 times finer output steps. This changes quantization, not the
CORDIC resolution or analog noise floor.

`set_phase_precision(bits)` accepts integers 0–8 and returns a boolean. The
choice is stored by **Save settings** (older configurations default to 0).
`get_precision_status()` reports requested/captured precision, radians per
count, state (0 settling, 1 live, 2 overrange, 3 DMA error, 4 sample gap), accepted/overflow/
DMA-error counters and processing/capture times in milliseconds. Saturated
and stale-scale captures clear the current spectrum and do not enter averages
or tracking. Reduce precision or bring the LO closer to the carrier when the
status reports overrange. Integer-domain drift removal preserves the extra
bits before spectral conversion to float.


ALPHA250-4 applies one precision to both streams. Metadata travels through
the asynchronous FIFOs with each sample and is committed at each DMA packet
boundary. A rate, precision or LO change restarts both filter histories and
unwrappers; settling discards queued samples. Upstream unwrap/subtractor
overflow is sticky within that epoch. Overrange automatically rebases the
paired pipeline at most once per second before acquisition resumes. The
reported radians-per-count value is for X; frequency-ratio scaling also
applies independently to Y.

The shared extractor retains 24-bit Cartesian mixer and prefilter outputs,
avoiding the carrier-dependent weak-PM gain error from 16-bit mixer rounding.
Reapplying the current channel, rate, precision, average target or LO tuning
word preserves acquisition and averaging history. Repeating the nominal LO
after tracking has moved it still restores the requested frequency.
Phase extraction calculates 24-bit CORDIC output before converting to the
legacy 16-bit phase unit with unbiased stochastic rounding. A dedicated
random generator per channel separates this conversion from mixer and I/Q
rounding. This avoids the deterministic phase staircase that produced coherent
LO harmonics; CIC precision alone could not remove those upstream errors.
Absolute phases accumulate in 64 bits and retain that width through frequency
scaling. The 65-bit pair difference is range-checked and saturated into the
32-bit CIC input. Thus a large common LO phase can cancel before any range
restriction, rather than overflowing an individual 32-bit phase accumulator
and clearing a valid cumulative average. True differential or packet range
loss still reports overrange. Packet formats and radians per count are unchanged.

Version 1.2.3 requires its cyclic-DMA FPGA and server to be deployed together.
It is incompatible with the former software-triggered packet design.

Version 1.2.4 retains the 30000/3000/300-point FFTs and frequency grid,
but consumes fresh 32768-sample windows instead of 65536 samples. The
30880 samples required by both FIR stages fit within that window. Phase
snapshots now contain 32768 samples per channel; clients should query
`get_phase_sample_count()`. The Python client queries this automatically
and retains compatibility with older 65536-sample instruments.
The browser polls at up to 60/s and counts only changed displayed spectra.
Actual FPS depends on acquisition, processing, network and drawing times.
ARM decimation uses NEON FIR evaluation and independent X/Y decimation
runs concurrently. FIR coefficients, timestamps, compensation and FFT
normalization are retained; SIMD accumulation can change float rounding.
The FPGA and device-tree overlay are unchanged from 1.2.3.

Build with:

```sh
make CFG=examples/alpha250-4/phase-noise-analyzer/config.mk
```

For Vivado 2026.1, add `VIVADO_VERSION=2026.1`. Deploy the server and FPGA
bitstream together: the normalized Q2.30 phase coefficients are incompatible
with the old integer-multiplier bitstream. The server restores DUT radians
and compensates the CIC's rate-dependent gain and downstream FIR scaling.

Each spectrum advances by half an FFT window. The XY cumulative
count reports processed segments, which overlap and are correlated. Configuration changes clear averages and
drain queued samples. Configuration holds both filters in reset, aborts the
old DMA epoch and restarts the descriptor chain on X.
Phase getters return the latest synchronized pair, with the integer unwrap
offset removed before float conversion. They return zeros before acquisition.

The two CICs admit live ADC-clock samples together. If either input is stalled
by its downstream FIFO, neither accepts the next live sample. One shared rate
source commits configuration to both CICs on the same clock. This prevents
independent FIFO draining from shifting the X/Y time axes and attenuating or
reversing the real cross spectrum of a shared phase-modulated signal.
On a rate change, the controller resets both CIC/FIR histories and FIFO queues
for 32 ADC clocks, configures both CICs together, then resumes paired sampling.
A cyclic scatter/gather DMA ring and hardware packet alternation drain both
FIFOs without a Linux rearm operation between packets. The reserved RAM contains
1024 descriptors and 512 complete X/Y packet pairs. A separate 1024-entry
hardware metadata ring retains each packet's precision and validity flags.
The reader withholds one completed pair until subsequent descriptor progress
confirms DDR writeback, validates descriptor completion/length and rejects
windows overtaken during copying.

This removes the software stalls that previously filled the FIFOs at CIC 50
and joined phase samples across missing ADC-clock intervals. A sticky hardware
flag also detects any CIC-input stall during a live epoch. Packets carrying
that flag clear the averages and report **Sample gap**; they do not enter the
FFT or tracking. Acquisition restarts together at most once per second.
`get_acquisition_status()` returns rejected gap captures, X/Y FIFO occupancies
and the current hardware gap flag. Hardware tests must establish the usable
rate under the actual DDR load; paired acceptance alone is not a lossless
sampling guarantee.

Phase-based block rejection is removed: quiet blocks, sparse quantized steps,
spikes and discontinuities all contribute to the spectrum. There are no jump,
peak/RMS or output-code rejection thresholds. DMA completeness and acquisition
epoch checks still prevent mixing incomplete or stale captures into averages.

Version 1.2.2 retains the Hann weights, FFT plans and work buffers across
captures. The 30000-sample level uses separate PFFFT complex transforms for
X and Y, running concurrently; the 3000- and 300-sample levels reuse Eigen
plans. Separate transforms preserve a quiet channel when X and Y have very
different amplitudes. Sample lengths, centering, one-sided density scaling,
FIR compensation, stitching and signed cross-spectrum averaging are unchanged.
This is a server optimization and uses the same FPGA design as 1.2.1.

Before version 1.3.0, the stitched spectrum contained 15001 bins with spacing `fs / 30000`, where
`fs = 200 MHz / (2 * CIC rate)`. Clients display offsets from two bins to 75%
of Nyquist. Jitter integration uses full decades within that band. A fitted
linear phase trend is removed before spectral processing; raw phase snapshots
and tracking telemetry retain the original samples. Tracking now estimates slope
from the current 32768-sample segment and controls X and Y independently in XY mode.

The FPGA prefilter is four cascaded 16-sample moving averages, equivalent to a
61-tap FIR with unity DC gain. Intermediate sums retain full precision; the
final 24-bit output uses stochastic rounding. Each channel uses separately
seeded 64-bit XOR LFSRs for mixer/filter rounding and CORDIC phase rounding.
Shared LFSR defaults keep other instruments' previous recurrence.

At 200 MS/s, the filter attenuates the 20 MHz mixing image by 57.27 dB, versus
2.28 dB for the former boxcar. Its passband loss is 0.091 dB at 500 kHz,
0.365 dB at 1 MHz and 1.470 dB at 2 MHz. Software does not invert this response.
The filter is intended for 10 MHz carriers and sub-MHz offsets; wider offsets
or low carriers need the response taken into account. At CIC 67, one final
FPGA phase output code is approximately 2.331 mrad.

The browser provides one selected spectrum, with optional 0.1-decade smoothing.
It averages signed linear values before displaying magnitude in dB; red markers
identify negative estimates. Nonpositive decade averages and integrated jitter
remain unavailable. CSV exports retain signed phase PSD and all four DDS
frequencies. Python captures also retain `phase_psd`; spur removal is opt-in.

The workspace follows the compact FFT interface: acquisition and local
oscillators above a full-width spectrum, phase/frequency controls, CSV/PNG
exports and jitter readouts. Drag zooms; double-click resets. All four nominal LO fields support Hz/kHz/MHz/GHz units and digit tuning with
arrow keys or the mouse wheel. Enter or blur applies valid edits; Escape restores
the accepted value. Tracking corrections are displayed separately from nominal
frequencies. CIC and rolling-average controls accept integers only. XY reports
cumulative synchronized windows and provides an explicit reset.

Capture ref retains a copy of the signed PSD, its frequency axis, acquisition
settings and receipt timestamp. CSV exports contain signed live/reference PSD
and all four applied LO frequencies; PNG exports include acquisition labels.
The plot polls the latest published PSD at up to 20 Hz, excludes cached replies
from its FPS counter, and pauses when hidden. Controls refresh at 2 Hz.
Connection failures disable controls and mark readings stale; leaving the page
closes its connections. A separate short publication lock lets PSD readers
continue while the next stitched FFT is processing. The four-input FPGA, phase
scaling and signed stitched-spectrum estimator are unchanged by this port.
The Python client exposes `get_nominal_frequencies()` and
`get_average_status()`; a zero average target denotes cumulative XY averaging. The shared INI parser now keeps trimmed storage alive while
restoring numeric and boolean settings.

The earlier distinct-pair LO mitigation and subsequent root-cause investigation
are recorded in the [hardware validation notes](tests/hardware-validation.md).
Signed cross-spectrum estimates are retained; the phase-extraction correction
does not replace negative values by magnitudes or establish an absolute noise
floor. FIFO timing under pressure and close-offset response remain separate
measurement limitations.

Run the software and FPGA regressions described in [tests/README.md](tests/README.md).
The calculation audit compares the production C++ pipeline with independent
SciPy calculations, including signed cross spectra and known modulation power.
The 24-bit Cartesian bitstream passed Vivado 2025.1 routing at 200 MHz with
setup slack +0.046937 ns and hold slack +0.012307 ns, using 29,401 LUTs,
37,666 registers, 109 block RAM tiles and 94 DSPs at placement. This revision
has build, RTL, model and software validation; board validation remains pending.
The earlier 16-bit Cartesian prefilter image was checked on the board and in
the browser with a shared 10 MHz oscillator on IN0/IN2 and ALPHA250 DAC0 on IN1/IN3. Detailed measurements and
prior implementation results are in [hardware validation notes](tests/hardware-validation.md).

Remaining limitations include absolute ADC/DAC phase-noise and carrier-power
calibration, close-offset detrending response, the CIC 4 scaling discrepancy,
frequency-dependent residuals, shutdown hangs observed during testing, and
sample timing if downstream FIFOs stall. Spectrum and settings are separate
RPC reads and can disagree during a configuration change. Existing checks
establish calculation consistency, not an absolute instrument noise floor.

### Live sample coverage

The header beside FPS and connection status shows recent **Coverage**, with 100% as the target.
It measures sample time included in accepted FFT windows after decimation,
counting overlap once. The display uses approximately ten seconds of acquisition
updates; its tooltip includes the total since acquisition reset. Queued data is
excluded until analyzed or skipped. Settings changes restart the coverage history.
A skipped FFT hop can still leave full sample coverage when adjacent windows
cover its samples. This percentage is separate from the number of independent
averages and from hardware sample-gap reporting.

### Processing capacity

The header's **Queue** badge shows queued sample time. Hover it for required and
estimated processing rates, buffer retention and shared stage timings. Amber
indicates insufficient estimated capacity or a queue beyond half its buffer.
Every accepted FFT enters the average; spectrum/jitter publication is capped at
30 Hz. Shared native-order averaging, retained buffers and ARM window preparation
are documented in the [shared PNA processing guide](../../../server/drivers/phase-noise/README.md#processing-capacity-and-display-cadence).
