This instrument measures the phase difference between IN0/IN1 (X) or IN2/IN3
(Y), or their cross-spectrum (XY). Channel selectors are X = 0, Y = 1, XY = 2.
The server returns a one-sided phase PSD in rad²/Hz. Positive estimates convert
to single-sideband phase noise with `10 * log10(PSD / 2)`; frequency-noise
density is `f² * PSD` in Hz²/Hz.

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
count, state (0 settling, 1 live, 2 overrange, 3 DMA error), accepted/overflow/
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

Build with:

```sh
make CFG=examples/alpha250-4/phase-noise-analyzer/config.mk
```

For Vivado 2026.1, add `VIVADO_VERSION=2026.1`. Deploy the server and FPGA
bitstream together: the normalized Q2.30 phase coefficients are incompatible
with the old integer-multiplier bitstream. The server restores DUT radians
and compensates the CIC's rate-dependent gain and downstream FIR scaling.

Each spectrum consumes a fresh, non-overlapping DMA window. The XY cumulative
count reports processed windows. Configuration changes clear averages and
drain queued samples; DMA configuration is applied between complete X/Y pairs.
Phase getters return the latest synchronized pair, with the integer unwrap
offset removed before float conversion. They return zeros before acquisition.

The two CICs admit live ADC-clock samples together. If either input is stalled
by its downstream FIFO, neither accepts the next live sample. One shared rate
source commits configuration to both CICs on the same clock. This prevents
independent FIFO draining from shifting the X/Y time axes and attenuating or
reversing the real cross spectrum of a shared phase-modulated signal.
On a rate change, the controller resets both CIC/FIR histories and FIFO queues
for 32 ADC clocks, configures both CICs together, then resumes paired sampling.
Backpressure can still discard shared ADC-clock instants; paired acceptance
preserves X/Y alignment rather than guaranteeing lossless sampling.

Phase-based block rejection is removed: quiet blocks, sparse quantized steps,
spikes and discontinuities all contribute to the spectrum. There are no jump,
peak/RMS or output-code rejection thresholds. DMA completeness and acquisition
epoch checks still prevent mixing incomplete or stale captures into averages.

The stitched spectrum contains 15001 bins with spacing `fs / 30000`, where
`fs = 200 MHz / (2 * CIC rate)`. Clients display offsets from two bins to 75%
of Nyquist. Jitter integration uses full decades within that band. A fitted
linear phase trend is removed before spectral processing; raw phase snapshots
and tracking telemetry retain the original samples. Tracking estimates slope
from 32000 samples and controls X and Y independently in XY mode.

The FPGA prefilter is four cascaded 16-sample moving averages, equivalent to a
61-tap FIR with unity DC gain. Intermediate sums retain full precision; the
final 16-bit output uses stochastic rounding. Each channel uses a separately
seeded 64-bit XOR LFSR for mixer and filter rounding. Shared LFSR defaults keep
other instruments' previous recurrence.

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

Run the software and FPGA regressions described in [tests/README.md](tests/README.md).
The calculation audit compares the production C++ pipeline with independent
SciPy calculations, including signed cross spectra and known modulation power.
The final prefilter bitstream passed Vivado 2025.1 routing at 200 MHz with
setup slack +0.149 ns and hold slack +0.024 ns, using 18,227 LUTs, 23,636 registers
and 94 DSPs. Board operation and the browser were checked with a shared 10 MHz
oscillator on IN0/IN2 and ALPHA250 DAC0 on IN1/IN3. Detailed measurements and
prior implementation results are in [hardware validation notes](tests/hardware-validation.md).

Remaining limitations include absolute ADC/DAC phase-noise and carrier-power
calibration, close-offset detrending response, the CIC 4 scaling discrepancy,
frequency-dependent residuals, shutdown hangs observed during testing, and
sample timing if downstream FIFOs stall. Spectrum and settings are separate
RPC reads and can disagree during a configuration change. Existing checks
establish calculation consistency, not an absolute instrument noise floor.
