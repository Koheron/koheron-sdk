Hardware validation notes — 2026-09-30 to 2026-10-01

These notes preserve measurements taken during development. Results preceding
the prefilter section used the old four-sample boxcar and, where stated, the
former phase-block rejection rule. They are historical evidence, not a
calibration of the final instrument. See [the instrument README](../README.md)
for current behavior and limitations.

Paired acquisition validation — 2026-10-01 after the V1 integration:
one AWG output with a 10 MHz carrier and 1° peak sinusoidal PM at 10 kHz was
split into IN1/IN3; the shared crystal remained on IN0/IN2. The raw ADC PM
phases agreed within 0.0124°, and no input clipped. The former acquisition
nevertheless shifted the phase streams by 54.33°, changing to 94.33° after
an instrument reload. A 302-window real cross spectrum consequently recovered
only 0.7616° peak and -43.55 dBc integrated tone power, while X/Y individually
recovered approximately 0.998°.

The paired controller now admits the same ADC-clock instants to both CICs.
It also resets both CICs, FIR data vectors and FIFO queues on rate changes,
holds reset for 32 ADC clocks, configures both rates together and resumes
sampling only afterward. This removes both independent-input drift and the
remaining half-output-sample offset seen without a common filter reset.
The DMA register layout, RPCs and signed cross-spectrum calculation are unchanged.

The hardware regression passed CIC 20, 67, 100 and 133, repeated rate changes,
and two instrument reloads. Mean X/Y modulation-phase differences were below
0.04° across the sweep; the largest individual snapshot difference was 0.324°.
At CIC 133, 64 fresh phase pairs gave X = 0.99767°, Y = 0.99765° and
XY = 0.99766° peak by integrated tone power. A separate live 302-window XY
average recovered 0.99769° and -41.203 dBc, versus -41.183 dBc for an ideal
1° modulation. Tracking stayed locked throughout that capture. Production
C++ and independent SciPy processing differed by at most 0.0031% complex RMS
on these same pairs. These checks validate this modulation's alignment and
scale; shared backpressure can still discard ADC-time samples.

Vivado 2025.1 strict routed timing and all 12 bus-skew constraints passed at
200 MHz: setup slack +0.135794 ns, hold slack +0.039732 ns. RTL regressions
passed asymmetric stalls, four reset/configuration epochs, a rate superseded
during reset and the prior block/prefilter tests. The deployed bitstream SHA256
is `c7d56a5b9882c97a05d047a5195ba9dbe548de4ee3fa4ef9cfa95ffd8a27c0dc`.

Validation on 2026-09-30: ARM server and TypeScript builds passed; software
regressions passed with ASan/UBSan; the Vivado 2026.1 bitstream build completed
for `xc7z020clg400-2`, with routed setup slack +0.156 ns and hold slack +0.025 ns.
No constrained endpoints failed. ADC pin timing, calibration and tracking still
need verification on an ALPHA250-4 board.

Hardware validation on 2026-10-01 used an ALPHA250-4 with a shared 10 MHz
oscillator on IN0/IN2 and an ALPHA250 DAC0 on IN1/IN3. After reference
attenuation, raw captures at 200 and 250 MS/s showed no clipped samples.
The Vivado 2025.1 image booted; routed setup slack was +0.071 ns and hold slack
+0.024 ns. The minimum-frequency readout was verified at 333.33 Hz (CIC 20)
and 833.33 Hz (CIC 8). Nineteen consecutive CIC configuration changes completed
with a maximum RPC round-trip time of 13 ms after fixing DMA lock handoff.

The software decimator now evaluates only retained FIR outputs and caches
sample-rate-independent response corrections. Reference-filter regressions
verify the retained samples and correction weights. Hardware cross-spectrum
throughput increased from approximately 4.5–4.8 to 12 accepted windows/s at
CIC rates 8, 20 and 100; processing still limits throughput at the lower rates.

Phase conversion compensates the CIC's rate-dependent gain and the FPGA FIR's
quarter-scale output, replacing the fixed empirical factor. The programmable
CIC output shift is described in [AMD PG140](https://docs.amd.com/api/khub/documents/HKoOZREGkL1fIIbSvCIguA/content).
A controlled 1 kHz DAC frequency step was measured within 0.02% at CIC 8, 20
and 100. With 10 MHz on IN0/IN2 and 11 MHz on IN1/IN3, the same step produced
the expected 909.09 Hz scaled change within 0.01% on both channels. CIC 4
showed a larger discrepancy and needs separate investigation.

Phase snapshots start at zero on each channel: the integer unwrap offset is
removed before conversion to float. This preserves small phase increments
after large accumulated offsets or carrier changes. Tracking estimates the
phase slope using the complete 32000-sample block rather than two endpoints.
The lock detector filters frequency error at the loop bandwidth, enters lock
below `0.1 * tracking_max_step`, and exits above twice that tolerance. Settings
and averaging resets clear its history; a stopped DMA producer clears lock.
At this stage of testing, rejected phase blocks also cleared lock. XY mode tracks each channel independently and reports lock
only when both loops lock. Following a two-second X acquisition and a switch to
XY, both DDS frequencies converged within 2 mHz; lock held throughout the
settled 15 seconds of a 30-second check, with 365 accepted windows.
With the former phase-block rejection enabled, disabling DAC0 cleared lock
and stopped accepted-window growth; restoring the
10 MHz output resumed averaging and reacquired lock within two seconds.
Longer tracking runs and absolute noise-floor calibration remain hardware
validation tasks.

A same-source-per-pair check used the oscillator split into IN0/IN1 and DAC0
split into IN2/IN3, with no clipped samples at 200 or 250 MS/s. At CIC 100,
tracking disabled and 64-window moving averages, the pair residuals were:

| Offset band center | X: oscillator (dBc/Hz) | Y: DAC0 (dBc/Hz) |
| --- | ---: | ---: |
| 1 kHz | -126.9 | -113.9 |
| 10 kHz | -135.9 | -125.0 |
| 100 kHz | -136.2 | -131.2 |

These are linear-power averages over each band from center/sqrt(2) to
center*sqrt(2), converted with PSD/2, with no spur removal. They describe
the complete pair residual, including splitter/cables and signal-dependent
artifacts, rather than an independently calibrated single-ADC floor. The
236-window XY cross check retained signed values and averaged near zero in
these bands; a negative estimate is not a measured negative noise power or
a calibrated cross-correlation floor.

The DAC-fed residual depends strongly on carrier frequency: changing both
Y local oscillators and DAC0 from 10.000 to 10.001 MHz reduced the 1 kHz
band from approximately -114 to -131 dBc/Hz. At 10.010 MHz it was -128
dBc/Hz; returning to 10.000 MHz reproduced approximately -114 dBc/Hz.
The 100 kHz band instead worsened at 10.001 MHz, so this is not a uniform
improvement. Swapping the two complete source/splitter paths between pairs
is still needed to distinguish source-path effects from ADC-pair effects.
DAC0 was restored to 10 MHz and XY tracking re-enabled after the check.

Client-side spectrum averaging retains signed cross-spectrum estimates until
after averaging in linear units. The browser plots magnitude in dB, with red
markers identifying negative raw or smoothed estimates. Magnitude is taken only
after signed averaging; negative estimates are not positive noise-power
measurements. Nonpositive decade-table averages remain unavailable. Browser
CSV exports include the signed phase PSD and all four DDS frequencies; Python
segment captures include `phase_psd`. Python spur removal is opt-in. Frequency
noise uses `f² * phase PSD` in Hz²/Hz, with matching browser labels and table
units. Browser plotting is limited to 10 Hz, control/measurement polling to
approximately 4 Hz, and control edits cannot create duplicate polling loops.
XY carrier-power readout uses IN0 and its calibration, consistently with X.

Remaining work includes investigating the CIC 4 calibration discrepancy,
server shutdown hangs observed on the board, frequency-dependent residual
artifacts, and behavior during runtime frequency changes. Spectrum and settings are still separate RPC reads; an atomic
measurement snapshot would prevent mismatched metadata during configuration
changes. Absolute carrier-power and ADC noise-floor calibration remain open.

The client corrections passed eight Python regressions, native C++ ASan/UBSan
regressions, serialized-payload/browser regressions and the ARM/web build.
On the original wiring, live XY throughput remained approximately 12.1
accepted windows/s. Browser verification exercised the phase/frequency toggle,
matching legends/table units, and a 15001-bin CSV containing all DDS metadata
and 7447 negative cross-spectrum bins. These checks validate data handling,
not the absolute noise or carrier-power calibration.

Spectral processing removes a fitted linear phase trend before building the
three-rate spectrum; phase snapshots and slope telemetry retain the original
samples. This prevents a constant carrier-frequency mismatch from dominating
the near-carrier PSD. On the original wiring, an approximately 3.6 Hz mismatch
with tracking disabled produced 26.7 dB less power in the 100 Hz band after the
change (82 accepted windows per capture). This is rejection of an offset
artifact, rather than a calibrated improvement in hardware noise floor.
Detrending affects the response in the first few bins: a standalone Hann-window
test gives approximately +0.43 dB for a two-bin sine, while its eight-bin tone
is preserved within 0.1% in power. The stitched spectrum's close-offset
response still requires calibration.

Jitter integration now selects full decades inside the same usable band shown
by clients: from two frequency bins to 75% of Nyquist. A live CIC 50 check
previously integrated from 100 Hz despite a 133.33 Hz minimum; it now uses
1 kHz to 100 kHz. CIC 100 retains its 100 Hz to 100 kHz interval. Signed
cross-spectrum integration can still produce an unavailable jitter estimate
when the finite-average integrated power is nonpositive.

Chrome verification exercised both plot units, trace visibility and legend
updates, acquisition-rate edits, and fractional DDS readouts. Display
decimation keeps frequency order and gaps in the plotted data. DDS fields
display millihertz precision, and plot controls wrap with consistent spacing.

With the added detrending, a five-second live check with Chrome connected
processed approximately 11.1 accepted XY windows/s, versus approximately
12 windows/s before detrending.

The browser workspace follows the compact ALPHA250 FFT interface (#700):
acquisition and all four local oscillators above a full-width spectrum, phase
and frequency units in segmented controls, CSV/PNG actions beside the plot,
and jitter readouts below it. Signal-path explanations expand separately.
The first spectrum autoscales vertically; drag zooms and double-click resets.
Valid local-oscillator edits apply on Enter or blur; invalid drafts stay visible,
and Escape restores the last accepted value. Explicit Set buttons also remain.

A numerical calculation audit compared production C++ spectra with independent
SciPy calculations on synthetic signals and 32 simultaneous phase snapshots
at CIC 67 (fs approximately 1.493 MHz). Complex RMS differences were below
0.001% in all three stitched frequency segments. Known phase-modulation tones
at 266.7 Hz, 3.333 kHz and 33.333 kHz recovered phase variance to within 0.001%.
Positive, negative and quadrature correlations and signed cumulative averages
passed regression checks. On the same captured samples, enabling linear phase
detrending left the negative-bin fractions above 10 kHz unchanged (49.75% in
10–100 kHz and 49.25% above 100 kHz for the 32-frame mean). This checks software
consistency, not absolute ADC/DAC phase-noise calibration or front-end artifacts.

The FPGA phase front end now replaces the original four-sample boxcar with
four cascaded 16-sample moving averages. Each stage retains its complete signed
sum; only the final output returns to 16 bits, with stochastic rounding. The
61-tap equivalent FIR has unity DC gain and identical delays on all I/Q and
X/Y paths. It uses adders and registers rather than additional DSP multipliers.
At 200 MS/s, rejection of the 20 MHz image for our 10 MHz carriers improves
from 2.28 dB to 57.27 dB before CORDIC phase extraction.

This filter is intended for the current 10 MHz, sub-MHz offset measurement.
Its amplitude response is down 0.091 dB at 500 kHz, 0.365 dB at 1 MHz and
1.470 dB at 2 MHz; the phase-noise density has the same loss expressed in dB
under a small phase modulation model. No inverse response correction is
applied in software. Wide-band CIC settings still expose filtered frequencies,
and a low carrier whose mixing image falls in the passband needs a different
filter configuration. This change is not an absolute noise-floor calibration.

Each channel selects a distinct 64-bit XOR LFSR state, with maximum-length
taps 64,63,61,60 from AMD/Xilinx XAPP052. Bit 0 controls mixer rounding;
separate 16-bit slices control final I and Q filter rounding. All channels
reset together but no longer receive identical rounding sequences. Shared
LFSR defaults preserve the previous behavior of other instruments.

Phase-based block rejection has been removed. Sparse quantized steps, spikes
and discontinuities now contribute to the spectrum and its averages rather
than causing the whole block to be discarded. The previous jump, peak/RMS
and output-code thresholds are no longer used. At CIC 67, one final FPGA
output code is approximately 2.331 mrad; removing rejection does not change
that resolution.

Vivado 2025.1 synthesis and routing pass at 200 MHz, with setup slack +0.149 ns
and hold slack +0.024 ns. The placed design uses 18,227 LUTs, 23,636 registers
and 94 DSPs. RTL simulation compares 100,000 input samples against exact
convolution and checks all six rounding-generator pair correlations. An ideal
mixer model recovers small phase modulation with the expected filter response
at 100 kHz, 500 kHz and 1 MHz. Hardware snapshots at CIC 67 retain signed
cross spectra; a preliminary 32-frame comparison reduced median cross-spectrum
residual magnitude above 100 kHz by about 2.5 dB while the negative-bin fraction
remained near one half. This is a short measurement comparison, not a calibrated
DAC phase-noise result.

Restart testing also found a shared INI parser lifetime bug: assigning the
temporary string returned by `trim()` to a `string_view` left numeric and bool
parsing reading expired storage. AddressSanitizer reproduced this as a
stack-use-after-scope while restoring the saved tracking flag. The parser now
owns the trimmed string for the duration of each parse. Regressions cover
saved boolean, float and integer settings, whitespace, long decimal values
and invalid numeric suffixes.


## 2026-10-05 ALPHA250 PNA workspace port

Boards: ALPHA250 DAC0 at 192.168.1.105 supplies both IN1/IN3 on the
ALPHA250-4 at 192.168.1.12. The independent 10 MHz reference supplies IN0/IN2.
Both boards run V1 PNA packages based on SDK commit `176be15d`; the receiver
software update is package 1.1.0. Its FPGA binaries and overlay are byte-identical
to the verified V1 package installed earlier that day. Source changes are on `feat/alpha250-4-pna-improvements`.

The source uses a 10 MHz carrier and sine PM. At CIC 133, five fresh synchronized
phase fits and at least 25 cumulative windows per setting yielded:

| PM offset | Commanded peak | Fitted X peak | Fitted Y peak | Integrated XY equivalent peak |
| --- | --- | --- | --- | --- |
| 10 kHz | 0.1° | 0.099710° | 0.099689° | 0.099668° |
| 10 kHz | 0.3° | 0.299763° | 0.299683° | 0.299725° |
| 1 kHz | 1° | 1.000017° | 1.000285° | 0.999741° |
| 10 kHz | 1° | 0.999953° | 1.000166° | 0.999980° |
| 10 kHz | 3° | 3.000935° | 3.000705° | 3.000773° |
| 50 kHz | 1° | 0.997807° | 0.997813° | 0.997810° |

The integrated XY result uses the signed PSD around the modulation line, with
peak phase computed as `sqrt(2 * integrated_power)`. Negative estimates remain
signed in CSV and reference storage. The zero-PM fitted baseline was below
0.0006° on both channels; this is not an absolute noise-floor calibration.
DAC0 was restored to 10 kHz sine PM, 1° peak; DAC1 remains disabled. Receiver
settings are restored to XY, CIC 133, with saved nominal frequencies unchanged.

The server/browser builds, Python tests, ASan/UBSan native regressions, seven
browser workspace tests, serialized-payload checks and independent calculation
audit pass. Browser checks include reference capture, signed CSV and PNG
exports, smoothing and frequency-noise conversion, stale-state handling during
instrument restarts and Retry reconnection. New nominal-frequency and average
status RPCs were exercised live: rolling status reached `(4, 4)` and cumulative
XY reported a positive count with target zero.

For 120 sequential PSD reads at CIC 133, the old server's median/p95/p99 were
1.55/71.54/73.41 ms. The publication cache measured 1.84/3.59/11.21 ms with the
browser active. These timings include network and host overhead and are not a
hard real-time guarantee. With the final 20 Hz polling cap and active browser,
the median/p95/p99 were 1.48/2.21/3.04 ms. A 60 Hz browser poll target overloaded the faster
CIC 67 acquisition in one repeat, while the same check passed with the browser
closed. Polling is therefore capped at 20 Hz for this paired-DMA receiver;
cached replies are excluded from displayed FPS. At 20 Hz, CIC 67 passed one
repeat but failed another phase-fit check (0.941° versus 1° commanded), despite
closely aligned channels and an integrated XY equivalent peak of 0.9971°.
This cap reduces load but does not establish a uniform time axis at CIC 67. The ALPHA250-4 stitched signed
estimator is retained instead of replacing it with ALPHA250's single-channel
Welch backend.

CIC 20 failed the fixed-10-kHz phase-fit criterion with both the previous and
updated servers, despite closely aligned X/Y phases and integrated XY tone
power within 3% of the command. This run therefore does not validate the fastest
rate's uniform time axis. Shared FIFO backpressure can discard ADC-clock sample
instants; the cause of this particular failure is not established. Use the
validated CIC 133 setting for this PM experiment.

Raw NPZ captures, JSON measurements, deployment hashes, instrument/settings
backups and browser exports are under `tmp/pna-pm-port-20261005` in the main
workspace; software build/test logs are under `tmp/pna-port` in the port worktree.
