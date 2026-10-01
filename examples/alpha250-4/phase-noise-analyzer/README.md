This instrument measures the phase difference between IN0/IN1 (X) or IN2/IN3
(Y), or their cross-spectrum (XY). Channel selectors are X = 0, Y = 1, XY = 2.
The server returns a one-sided phase PSD in rad²/Hz. Positive estimates convert
to single-sideband phase noise with `10 * log10(PSD / 2)`; frequency-noise
density is `f² * PSD` in Hz²/Hz.

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
exports and jitter readouts. Drag zooms; double-click resets. DDS fields retain
millihertz precision, apply valid edits on Enter or blur, and restore the last
accepted value on Escape. Plot polling is limited to 10 Hz and controls to
approximately 4 Hz. The shared INI parser now keeps trimmed storage alive while
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
sample alignment if downstream FIFOs stall. Spectrum and settings are separate
RPC reads and can disagree during a configuration change. Existing checks
establish calculation consistency, not an absolute instrument noise floor.
