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

## Runtime precision validation — 2026-10-05

PNA 1.3.0 on ALPHA250 `192.168.1.105` drives DAC0 into ADC1/ADC3 of
ALPHA250-4 `192.168.1.12` (PNA 1.2.0). ADC0/ADC2 receive the 10 MHz reference.
DAC0 is a 10 MHz carrier with 1° peak sinusoidal PM at 10 kHz; DAC1 is muted.
CIC rate 133 gives 751879.718 Hz phase sampling. XY averaging and slow
tracking remain enabled. Both full FPGA/server/web packages were rebuilt and
all installed file hashes checked; existing settings and the generator words
were preserved during installation.

Software/RTL checks passed separately from these board measurements: native
ASan/UBSan acquisition/DMA/scaling suites, Python RPC tests, web tests,
quantizer signed ties/saturation/all nine precisions under stalls, and paired
controller/FIFO metadata regressions. Strict routed timing checks passed:
ALPHA250 setup 0.071 ns, hold 0.000 ns; ALPHA250-4 setup 0.192 ns, hold 0.039 ns.
The ALPHA250 hold margin is very small. The ALPHA250-4 IP status report marks
its custom cores up to date; generated RTL hashes match the checked sources.

Eight fresh, metadata-validated XY captures at every setting 0–8 recovered
0.99949–0.99982° mean peak PM in both streams. Maximum relative PM phase stayed
below 0.027°. A longer comparison used 80 distinct valid XY snapshots per
setting (Standard and +8). Standard's step was 2.437810 mrad/count; +8's was
9.522696 µrad/count, exactly 256 times finer. The measured band averages were:

| Offset band | Standard negative bins | +8 negative bins | Standard X auto PSD | +8 X auto PSD |
| --- | ---: | ---: | ---: | ---: |
| 20–50 kHz | 41.0% | 0.334% | -122.68 dBc/Hz | -142.22 dBc/Hz |
| 50–100 kHz | 40.1% | 0.702% | -123.10 dBc/Hz | -142.01 dBc/Hz |
| 100–280 kHz | 36.5% | 0.724% | -122.80 dBc/Hz | -142.04 dBc/Hz |

The 10 kHz modulation remained 0.99972–0.99975° in this longer comparison.
These results identify a substantial output-quantization limitation in this
setup; they do not establish an absolute analog noise floor or imply that all
negative cross-spectrum estimates disappear with higher precision. Signed
estimates are still retained rather than clipped or replaced by magnitudes.

A deliberate +300 kHz DUT-X LO offset at +8 caused hardware overrange;
`get_phase_snapshot()` marked the capture invalid, the spectrum was cleared,
and the overflow counter incremented. Restoring the nominal LO resumed valid
acquisition. No DMA errors occurred. +8 was saved on the receiver and survived
an instrument restart with all four nominal LOs unchanged. ALPHA250's runtime
controls and captured precision metadata were exercised at all nine settings;
its acquisition reported overrange with the present ADC inputs, so the
calibrated PM validation is limited to the wired ALPHA250-4 receiver.

Local evidence: `tmp/pna-precision/hardware/precision-sweep.json`,
`precision-comparison.json`, `precision-*-*.npz`, `overrange.json`,
`persistence.json`, deployment manifests, generator before/after settings,
and `precision-comparison.png`. The scripts and complete build/test logs are
under `tmp/pna-precision/`; these temporary artifacts are not committed.

2026-10-05 — remaining negative components after the precision port

The user's follow-up prompted a new check of the connected pair. The receiver
had +8 active, CIC 50, Y selected and unequal nominal frequencies within a
pair. The source retained the 10 MHz carrier and 1° peak, 10 kHz sine PM.
The ALPHA250 source's ADC acquisition was still overrange; only the wired
ALPHA250-4 receiver provides a valid analyzer measurement in this setup.

Restoring CIC 133, XY and four nominal LOs at 10.001 MHz did not eliminate
every negative component. One hundred fresh, distinct valid snapshots gave
0.999645° and 0.999653° peak PM, with maximum relative PM phase 0.00483°.
The remaining negatives included coherent narrow spurs, rather than only
uncorrelated finite-average residuals. For example, the 103.810 kHz spur had
coherence 0.972 and real cross magnitude -130.27 dBc/Hz. Moving all four LOs
from carrier +1 kHz to carrier +1.3 kHz moved that spur to 135.013 kHz;
another moved from 203.634 to 264.837 kHz. Those shifts correspond to the
104th and 204th harmonics of the 300 Hz LO change. This demonstrates
LO-dependent analyzer artifacts. Upstream phase quantization is a candidate,
but these measurements do not isolate the mixer, CORDIC and ADC contributions.
The CIC precision change does not increase the 16-bit CORDIC resolution.

Setting both X LOs to 10.001 MHz and both Y LOs to 10.0017 MHz reduced the
coherent negative spurs without changing the incoming signal or rectifying
the estimator. With tracking disabled, 100 fresh snapshots gave:

| Offset band | Equal pair frequencies: negative bins | Distinct pair frequencies: negative bins |
| --- | ---: | ---: |
| 20–50 kHz | 0.585% | 0.084% |
| 50–100 kHz | 1.654% | 0.100% |
| 100–280 kHz | 0.668% | 0.125% |

The strongest remaining negative in the distinct-frequency capture was
approximately -140.2 dBc/Hz with coherence 0.04, compared with approximately
-129.8 dBc/Hz and coherence 0.89 in the equal-frequency capture. The 1° PM
fit remained 0.999756° / 0.999762°, with maximum relative phase 0.00668°.

A separate 80-snapshot test applied uniform-noise PM with 0.2° configured
deviation and a 1 MHz update rate. The distinct-frequency setup returned no
negative bins from 1–280 kHz. Cross/auto integrated power ratios were
0.99878–0.99903 across the tested bands, and median coherence was
0.9980–0.9983. This tests recovery of a common broadband modulation above the
residual floor; it does not calibrate the absolute floor.

The generator was restored to its exact acknowledged sine-PM settings.
With slow tracking re-enabled, another 100 fresh snapshots recovered
0.999822° / 0.999836° and maximum relative phase 0.00734°. Negative fractions
were 0%, 0% and 0.167% in the three bands above; the live server's concurrent
199-window average gave 0%, 0% and 0.111%. Both tracking loops reported lock.
The receiver's distinct-pair LO settings, CIC 133, XY, +8 and tracking enabled
were saved. This is an operating mitigation, not a complete front-end fix.
Longer observation also found an overrange-triggered averaging reset with
unchanged configuration; continuous averaging stability remains unresolved.

Local evidence: `tmp/pna-negative-current/diagnose.py`, `matched-fixed.npz`,
`offset-1300.npz`, `offset-split.npz`, `split-broadband.npz`,
`split-tracking.npz`, their summaries, saved receiver/generator settings,
and `lo-artifact-comparison.png`. Temporary artifacts are not committed.

Root-cause correction — 2026-10-05, ALPHA250-4 1.2.1:

Two upstream FPGA defects were reproduced independently of the spectrum
estimator and of the distinct-pair LO mitigation:

- AMD's installed bit-accurate CORDIC model, with the instrument's 16-bit
  Cartesian inputs and rounding mode, produces deterministic phase errors.
  At an I/Q radius of 3000 codes, the 16-bit output has 1.2104e-4 rad RMS
  error against atan2 of those same integer inputs. Its 104th and 204th
  angular harmonics are 6.3587e-6 and 9.1916e-6 rad. These harmonics explain
  the LO-dependent coherent artifacts observed when varying the LO offset.
  Calculating 24-bit phase reduces those harmonics by 53.95 and 55.69 dB in
  the model. These are model error reductions, not calibrated hardware
  noise-floor improvements.
- Absolute phase accumulated in signed 32-bit registers before pair
  subtraction. A 3 kHz common LO offset reaches that range after
  `2^31 / (16384 * 3000) = 43.6907 s`, even though the differential phase
  remains small. The old board reported overrange at 43.8538 s and cleared
  its cumulative average with no configuration change.

The correction calculates 24-bit phase, then uses dedicated per-channel
random streams for unbiased rounding into the legacy phase unit. Retaining
that unit keeps the existing CIC input range and published radians per count.
Absolute phase and frequency scaling now retain 64 bits; subtraction retains
65 bits until an explicit saturating 32-bit differential range check. The
ALPHA250 design receives the same phase-extraction correction; its accumulator
already resets for each DMA acquisition.

The ALPHA250-4 passed strict routed timing: WNS +0.042042 ns, WHS +0.029840 ns,
and all 12 bus-skew checks. The installed package's FPGA, server and web hashes
match the local package; the server is active and FPGA reports operating.
Signed-rounding RTL tests exhaust all 256 fractional codes and all 256 random
values at three signed phase positions. Headroom tests reproduce the former
positive and negative overflow and check common-phase cancellation, saturation
and reset. Native sanitizer, Python, browser and spectrum calculation
regressions also pass.

Live comparison used the existing reference/DAC wiring, CIC 133, XY, +8 bits,
slow tracking, and **all four nominal LOs at 10.001 MHz**. Each firmware
contributed 100 distinct valid phase snapshots. The DAC remained a 10 MHz
carrier with 1 degree peak sinusoidal PM at 10 kHz.

| Offset band | Old negative-bin fraction | Corrected negative-bin fraction |
| --- | ---: | ---: |
| 20–50 kHz | 0.501% | 0% |
| 50–100 kHz | 1.203% | 0% |
| 100–280 kHz | 2.089% | 0% |

The corrected PM fits were 0.999698 and 0.999708 degrees; maximum X/Y relative
phase was 0.00604 degrees. The signed live server average likewise had no
negative bins in these three bands. Some close-offset estimates remain
negative; this test establishes removal of the coherent artifacts in the
stated bands, not universal positivity or an absolute floor calibration.

With all four nominal LOs at 10.003 MHz, 150 seconds of monitoring crossed
more than three former overflow intervals with no new overrange or DMA error.
The cumulative average increased monotonically. All nine precision settings
recovered the 1 degree PM tone within 0.00056 degrees in an eight-snapshot
check per setting. A deliberate 300 kHz error on one LO still reported
overrange and rejected the saturated snapshot; returning the LO restored
valid acquisition.

An 80-snapshot common broadband PM test used uniform noise with 0.2 degree
configured deviation and a 1 MHz update rate, again with equal nominal LOs.
No negative bins occurred from 1–280 kHz; band cross/auto integrated power
ratios were 0.99894–0.99906. The generator's exact acknowledged settings were
restored afterward.

Local evidence is in `tmp/pna-root-cause`: build/test logs, deployment hashes,
`overflow-repro.json`, `headroom-live.json`, `precision-sweep.json`, generator
settings and `phase-rounding-fix.png`. Identically processed before/after
phase snapshots and summaries are
`tmp/pna-negative-current/baseline-equal-1k.npz`, `fixed-equal-1k.npz` and
`fixed-broadband-equal.npz`. Vendor model files remain local and are not
redistributed.

ALPHA250 1.3.1 also passed strict routed timing (WNS +0.024815 ns,
WHS +0.038527 ns; all 10 bus-skew checks). Its instrument-specific
`Performance_ExplorePostRoutePhysOpt` strategy closes the existing DAC
controller path without relaxing clock constraints. FPGA, server and web
hashes were verified after installation on 192.168.1.105. Both DAC channels'
exact acknowledged settings match the pre-install settings. Precision requests
0–8 and rejection of 9 were checked over RPC; its ADCs are not connected to
the receiver wiring, so this is not a hardware calibration of ALPHA250's ADC
measurement path.

The receiver's equal nominal LOs, CIC 133, XY, +8 and enabled tracking were
saved and verified after an instrument reload. Its single deliberate
range-test overflow is expected; the long stability run had no overflows.

After both corrected packages were installed and the receiver reloaded,
a further 100 fresh snapshots recovered 0.999762 / 0.999772 degrees with
maximum relative phase 0.00510 degrees, no overrange and no DMA error.
The 20–50 and 50–100 kHz bands had no negative bins. The 100–280 kHz band
contained one negative bin (0.0139%) at 192.005 kHz, -151.37 dBc/Hz magnitude
and coherence 0.0113. For comparison, the strongest old negative spur in
20–280 kHz was -122.40 dBc/Hz with coherence 0.9671. This distinguishes the
removed coherent artifact from a residual small cross-spectrum estimate.
The final snapshots are `tmp/pna-negative-current/final-both-fixed.npz`.


FFT throughput validation — 2026-10-05, ALPHA250-4 PNA 1.2.2

With browser controls idle, the same connected pair was measured before and
after the server optimization. IN0/IN2 received the 10 MHz reference;
ALPHA250 at 192.168.1.105 supplied DAC0 to IN1/IN3 with 10 MHz carrier,
10 kHz sinusoidal PM and 1 degree peak deviation. Tests used XY, +8 bits,
all nominal LOs at 10.001 MHz and tracking enabled. Each throughput sample
covered 12 seconds after three seconds of settling; processing times are
medians of the receiver telemetry.

| CIC rate | Accepted windows/s, 1.2.1 → 1.2.2 | Processing ms, 1.2.1 → 1.2.2 |
| --- | --- | --- |
| 133 | 11.44 → 11.46 | 69.67 → 40.67 |
| 67 | 13.29 → 21.25 | 69.99 → 41.21 |

CIC 133 is limited by acquisition duration. CIC 67 showed approximately 60%
more accepted windows per second. These are short controlled observations,
not a guarantee of lossless ADC-time sampling under shared backpressure.
Neither run incremented overflow or DMA-error counters; the old instrument
already had one overflow from earlier control changes.

Before/after captures each contained 100 fresh synchronized snapshots at
CIC 133. Fitted PM peaks were 0.999717/0.999729 degrees before and
0.999738/0.999743 after, with maximum relative phase differences of
0.00654 and 0.00639 degrees. Independent double-precision SciPy calculations
and the new production C++ estimator agreed within 0.01% complex RMS in
each stitched segment on both captured sets. Signed values remain intact;
near-zero cross estimates can still be negative. The final capture had no
overflows or DMA errors.

Build checks: the ARM server compiled with strict warnings and NEON enabled;
web assets built; the complete software regression runner passed, including
ASan/UBSan, Python and browser tests and the independent SciPy oracle. The
single-window reference comparison also passed directly on the receiver's
ARM CPU, covering sample lengths 30000/3000/300/301, rate changes and Y/X
gains of 1e-6, 1 and 1e6.

Deployment reused the verified 1.2.1 FPGA bitstream and device-tree overlay
byte for byte; no FPGA source or constraints changed and no new routed timing
result is claimed. The installed bitstream `.bit.bin` SHA256 remains
`da2b0535dde2efe58e38bbb62e641d7e7c8de0bb5fa8a7f85568d7018e032798`.
All installed package files were checked by SHA256 and the saved INI was
preserved. The user's live CIC 50, channel Y, 14 averages, +8, four 10 MHz
nominal LOs and enabled tracking were restored. Both source DAC channels'
exact acknowledged settings stayed unchanged.

Local artifacts: `tmp/pna-throughput/controlled-{before,after}-rate.json`,
`captured-{before,after}-audit.log`, `deployment/deployment.json`,
`user-state-{before,restored}.json`, and
`tmp/pna-negative-current/throughput-{before,after}.npz`.

Continuous acquisition validation — 2026-10-05, ALPHA250-4 PNA 1.2.3

At CIC 50 the previous packet-by-packet DMA rearming filled the Y FIFO
to 32769 samples. The paired CIC then stopped admitting ADC samples.
Captured modulation phase jumped at each 8192-sample packet boundary:
the median absolute jump was 94.42 degrees. This produced the broad
rippled skirt around the 10 kHz PM tone. A whole-window fit recovered
only 0.410 degrees despite individual packets containing the expected
1 degree modulation. An independent spectrum calculation reproduced
the displayed artifact.

The receiver now uses hardware-alternating X/Y packets and cyclic SG DMA.
Neither packet boundaries nor Linux scheduling require DMA rearming.
Per-packet metadata carries a sticky hardware sample-gap indication;
flagged windows are discarded and the paired acquisition restarts.
The first live deployment also exposed a concurrent settings-change race:
the reader could observe the temporary ring stop before restart completed.
Its running-state check now takes the configuration mutex, with a native
regression covering an active reader during a delayed reconfiguration.

Hardware checks used the same connected pair and unchanged source DAC0:
10 MHz carrier, 10 kHz sinusoidal PM, 1 degree peak. Forty fresh paired
windows at each CIC rate 50, 4, 8, 20, 67, 90, 133 and 50 passed with
zero sample gaps, overflows and DMA errors. Mean per-packet PM peaks
were 0.999982–1.000154 degrees. Maximum within-window phase change
between packet fits was 0.14774 degrees over the full sweep; at CIC 50
the median was 0.00633 degrees and maximum 0.05625 degrees. A complete
CIC 50 window recovered 1.000004 degrees. FIFO observations during the
sweep stayed below 8154 samples, well below the previous full FIFO.

The entire receiver server was deliberately stopped with SIGSTOP for
one second, then resumed with SIGCONT. DMA and the ADC timeline continued
autonomously, including multiple DDR ring wraps at CIC 4:

| CIC rate | Packets transferred during pause | Maximum FIFO X / Y | Sample gap |
| --- | --- | --- | --- |
| 90 | 272 | 1 / 8177 | None |
| 50 | 488 | 206 / 8157 | None |
| 4 | 6112 | 3456 / 8185 | None |

After every pause, the receiver returned valid fresh spectra with zero
gap and DMA-error counters. These checks establish continuity under the
tested rates and pauses; other hardware failures remain detectable by
the gap metadata. They do not claim calibrated absolute noise floors.

Build checks: the complete ASan/UBSan software suite, 11 Python tests,
10 browser tests, independent SciPy oracle and RTL regressions passed.
The ARM server and web assets built successfully. Final strict routed
timing passed at the original clocks: WNS +0.006492 ns, WHS +0.024840 ns,
all 12 bus-skew constraints checked. The first route missed setup by
0.044011 ns inside the programmable CIC scaler. Committed post-route
physical optimization and the shared hold-fix hook close timing without
relaxing constraints. The final package used that checked bitstream and
the matching SG device tree; enabling physical optimization on the existing
implementation run did not change synthesized interfaces or constraints.
The installed `.bit.bin` SHA256 is
`abccd24478e1bea33db557ba44fb47d45829c13fb64303c702b3fe7b38f2af57`.

Forty captured windows at the restored settings also passed the independent
production-estimator/SciPy calculation audit. The refreshed browser showed
the narrow PM tone without the former broad rippled skirt, with live
precision status and tracking locked. All installed files were checked by
SHA256, and the saved INI remained byte-identical. The user's latest live
CIC 90, Y, 104 averages, +8 bits, four nominal 10 MHz LOs and enabled tracking
were restored. Both source DAC channels' exact acknowledged settings
remained unchanged.

Local artifacts: `tmp/pna-browser-strange/{current.npz,fifo-counts.json,packet-analysis.json}`
and `tmp/pna-dma-continuity/{live-summary.json,cic-*.npz,server-pause-cic*-summary.json,
current.npz,captured-audit.log,continuity-before-after.png,strict-bitstream.log,
software-tests-final.log,rtl-tests-final.log,deployment-final/deployment.json,
user-state-restored.json,source-after.json}`.

30000-point throughput validation — 2026-10-05, ALPHA250-4 PNA 1.2.4

The browser's explicit 20/s polling limit predated cyclic DMA. Its target
is now 60/s; identical cached replies remain excluded from displayed FPS.
The three FFT lengths, Hann windows, bin spacing, stitching and density
normalization remain 30000/3000/300 with the same sample-rate relationships.
Fresh acquisition windows are now 32768 samples, containing all 30880
samples needed by the FIR chain instead of waiting for 65536 samples.
Raw phase snapshots therefore contain 32768 samples per channel, exposed
by `get_phase_sample_count()`. Updated Python clients query the length and
fall back to 65536 for older instruments.

ARM FIR decimation evaluates four taps per NEON operation; startup samples
retain the scalar zero-history calculation. Double representations retain
the scalar implementation. Independent X/Y decimation runs concurrently.
The original FIR coefficients and output timestamps are unchanged; float
accumulation order changes. A direct ARM comparison against the scalar
reference passed for noise, impulses, ramps, tones and quantities, including
the double-precision fallback. With the receiver server paused to isolate
the benchmark, decimation fell from 5.024 to 4.003 ms per channel and the
cross-density pipeline from 28.960 to 26.687 ms before parallel decimation.

Observed live rates before and after the combined changes:

| CIC / channel | Accepted windows/s, 1.2.3 → 1.2.4 | Median processing ms, 1.2.3 → 1.2.4 |
| --- | --- | --- |
| 60 / Y | 25.35 → 38.89 | 27.07 → 23.81 |
| 50 / Y | 30.43 → 35.81 | 27.06 → 24.06 |
| 50 / XY | 21.77 → 24.27 | 40.42 → 37.71 |

These short observations include client load; browser polling increased
from 20/s to 60/s and reconnected during the first post-install run.
They are not maximum-throughput guarantees. A final eight-second run at
the user's restored CIC 60 / Y settings accepted 35.85 windows/s; the browser
showed 32 new spectra/s while polling at 60/s, with approximately 4.0 ms
read, 2.1 ms processing and 2.3 ms drawing per poll. Acquisition and DSP,
rather than the previous display cap, now limit the rate. 60 fresh FPS
has not been achieved.

Forty fresh paired windows at each CIC rate 60, 50, 4 and 133 recovered
0.999858–1.000061 degree peak PM, with maximum X/Y mismatch 0.07792 degrees
and zero overflows, sample gaps or DMA errors. Ten captured windows ran
through the actual ARM/NEON production estimator and independent SciPy
oracle; relative complex RMS error was below 0.014% in all three segments.
A one-second server SIGSTOP at CIC 60 transferred another 408 hardware
packets with FIFO maxima 143/8186 and no sample gap.

Build checks: strict ARM server and web builds passed, as did the full
ASan/UBSan suite, 12 Python tests, 10 browser tests and the independent
calculation oracle. Additional DMA tests cover 32768-sample fresh windows.
This is a software-only deployment: FPGA and overlay hashes match 1.2.3
byte for byte, with its previously verified strict timing; no new routed
timing result is claimed. Installed package files were verified by SHA256.
The saved INI and exact source DAC settings are unchanged. The user's
latest CIC 60 / Y, one average, +8 bits, nominal 10 MHz LOs, internal
reference clock and enabled tracking were restored, with both trackers
allowed to reacquire before returning to Y.

Artifacts are under `tmp/pna-30000-fps`: `{before,after}-rate.json`,
`final-state.json`, `live-summary.json`, `current.npz`,
`captured-arm-audit.log`, `isolated-profile.log`, `server-pause-summary.json`,
`software-tests-final.log`, `short-window-tests.log`,
`deployment/deployment.json` and `source-{before,after}.json`.

## Streaming Welch commonization (2026-10-06, PNA 1.3.0)

ALPHA250, ALPHA250-4 and Red Pitaya now share a streaming 32768-point
real-FFT engine, a 16384-sample hop and one single/paired cyclic DMA reader.
X/Y use rolling three-segment Hann estimates followed by their selected
moving average. XY accumulates each signed complex segment cross spectrum
once. Overlap correlates segments; doubling the update count does not imply
doubling the number of independent observations. The former quad software
multirate FIR/stitching pipeline is replaced; close-offset detrending and
averaging statistics change. The density grid is now 16385 bins at fs/32768.

PR #772's native-order accumulation and plan-derived publication permutation
are shared by the streaming and batch reference estimators. PR #773's GCC
ARMv7 vector memory-access changes and vendor build dependencies are included.
The paired worker and FFT plans persist, FFT input/output alias safely, and
ARM integer phase fitting is exact. SIMD spectrum operations retain scalar
fallbacks for extreme/subnormal operands. XY skips unused rolling means.
Raw snapshots are converted only on request. The DMA reader reuses overlap
while checking metadata and descriptors over the entire retained window.

Build/software checks:

- Strict ARM server and web builds pass for all three boards. Both ALPHA
  software suites pass with ASan/UBSan, Python and browser regressions.
- The PR #772 ordered/native batch comparisons and PR #773 independent DFT,
  Parseval, ordering and in-place checks pass with host SIMD and scalar builds.
- The new streaming oracle checks every segment and rolling auto/signed CSD
  estimate against independent SciPy calculations. It covers a 120 dB
  channel-power ratio, one-count steps on large offsets, nearly full-range
  positive/negative ramps, signed endpoints, history resets and the XY path
  without rolling means. Relative auto/complex errors remain below 3e-7.
  Host SIMD/scalar and Cortex-A9 NEON under QEMU pass. QEMU checks numerical
  behavior; live timings below are measured on the physical ALPHA250-4.
- DMA regressions check exact half-window overlap, retention loss, watchdogs,
  epoch changes and automatic rearming after DMA errors. A recovery bug found
  during the first live sweep is fixed: clearing the running flag no longer
  clears the request to restart acquisition.
- The complete Vivado 2025.1 quad build passes strict routed timing:
  WNS +0.046937 ns, WHS +0.012307 ns and all 12 bus-skew constraints.
  No FPGA source or constraints changed in this port.

Hardware measurements use 192.168.1.12, unchanged connected signals, +8 bits,
one moving average, four nominal 10.001 MHz LOs and enabled tracking. Baseline
1.2.5 runs cover eight seconds; final 1.3.0 runs cover ten seconds after three
seconds of settling. Processing values are medians of live telemetry.

| CIC / channel | Accepted updates/s, 1.2.5 → 1.3.0 | Processing ms, 1.2.5 → 1.3.0 | New ring overruns |
| --- | --- | --- | --- |
| 133 / XY | 22.99 → 45.88 | 36.02 → 11.04 | 0 |
| 67 / XY | 26.53 → 73.37 | 35.66 → 11.20 | 0 in this short run |
| 50 / XY | 26.31 → 72.55 | 36.15 → 11.19 | 2 |
| 50 / X | — → 79.47 | — → 9.94 | 2 |
| 50 / Y | — → 79.45 | — → 9.92 | 2 |

The new count measures overlapping segments, whereas 1.2.5 counted disjoint
windows. These rates measure update throughput, not independent-average
convergence. CIC 67/50 produce hops faster than the CPU can consume them;
queued data eventually overruns the ring. In this initial measurement, such
windows were rejected and the epoch restarted, incrementing the overrun and
DMA-error counters. The consumer-recovery fix documented below supersedes that
behavior; these measurements remain the pre-fix evidence.
The short CIC 67 run does not establish sustained coverage. Neither sweep
reported a hardware sample gap or phase overflow.

Separate 30-second XY checks at CIC 100 and 133 reached 61.05 and 45.90
segments/s respectively, with no new DMA errors, overruns, gaps or overflows.
Both reach their acquisition ceilings. CIC 100 or higher is appropriate for
continuous half-window processing under this tested load; other client/DDR
loads can change that limit. Hardware phase snapshots remain synchronized
32768-sample pairs with finite values and matching +8 metadata. This test does
not establish absolute noise-floor or analog PM calibration for the new
close-offset response. ALPHA250/Red Pitaya hardware validation of this
streaming port remains pending.

PNA 1.3.0 is installed and remains the default instrument. HTTP readback of
server, FPGA, overlay, app.js and version matches the final archive by SHA256.
Server SHA256: `4ab5df18a9f9a29a775c781c0b91d084cfb2c15d8832950ad0498798963227f9`.
Bitstream SHA256: `1808b9c36da9b7f53ea535e76862d2ae5c2ee2295c336f7964d685d77fdc9129`.
The final live controls are restored to CIC 133, XY, +8, one average, internal
reference and the four nominal 10.001 MHz LOs. Tracking stays enabled.

Local artifacts are in `tmp/pna-streaming`: baseline/final rate JSON, sustained
checks, synchronized snapshots, deployment hashes, final state, build/test
logs and the ARM/SciPy oracle results. The previous working 1.2.5 archive is
retained separately for rollback.

### Consumer-overrun recovery (2026-10-06)

Chrome exposed repeated average resets at CIC 30, Y, 60 moving averages and
+8-bit precision. Read-only telemetry reproduced the count falling from 60 to
25/31/0 while consumer-overrun and DMA-error counters advanced together; FPGA
sample-gap and overflow counters stayed zero. CIC 30 produces 203.45 half-window
hops/s, exceeding measured CPU throughput. Restarting the DMA epoch for a lost
consumer window incorrectly discarded valid earlier averages.

The shared ring reader now resumes at the latest complete window on the same
hop grid and returns the number of skipped hops. It preserves the hardware
generation, validates every descriptor and metadata item, and never joins
samples across the missing interval. The estimator's three-segment history
resets across that interval, while valid outer moving/cumulative averages stay
intact. Tracking advances by one observed hop instead of integrating an
unobserved interval. Actual DMA errors, FPGA sample gaps, overflow and settings
changes retain their invalidation behavior. Chrome displays `Live · Skips`;
its tooltip reports consumer-overrun events since instrument start.

Build/software checks: all three ARM server/web builds pass. Both ALPHA test
suites pass, including ASan/UBSan and independent numerical checks. Production
quad overload regressions exercise X and XY through three overruns each and
assert monotonic average counts, unchanged producer generation and unchanged
DMA-error counts. Both shared DMA variants check contiguous recovery, explicit
skipped-hop accounting and normal overlap on the following read. Browser
precision/status regressions pass on all three boards. No FPGA change or new
FPGA timing run was needed for this recovery fix.

Live tests on 192.168.1.12 used the current connected signals, +8-bit precision,
60 moving averages, nominal 10.001 MHz LOs and enabled 0.1 Hz tracking. Runs start
after three seconds of settling. The user's current CIC 100/Y selection was
captured immediately before deployment and restored after the CIC 30 stress test.

| CIC / channel | Duration | Accepted segments/s | Average count | New consumer overruns | New DMA errors / hardware gaps / overflows |
| --- | ---: | ---: | --- | ---: | --- |
| 100 / XY | 39.74 s | 61.05 | 174 → 2599, no decreases | 0 | 0 / 0 / 0 |
| 100 / Y | 39.92 s | 61.03 | 60 throughout | 0 | 0 / 0 / 0 |
| 30 / XY | 29.81 s | 61.53 | 189 → 2023, no decreases | 17 | 0 / 0 / 0 |
| 30 / Y | 29.94 s | 70.36 | 60 throughout | 16 | 0 / 0 / 0 |

All sampled publications stayed Valid, and tracking was locked at each run's
end. Retaining averages fixes recovery; CIC 30 still loses temporal coverage
when the CPU cannot consume every hop. No known-amplitude modulation or absolute
phase-noise calibration was performed in this recovery test.

Deployed archive SHA-256:
`b5df64f4d4fbb31c1d3fdafb4a5f5f65aca3b8858e2b808fedc2fe0b2a537849`.
The live `serverd` SHA-256 is
`d023cb0bfb1df8625f0cdb49e82056e577099def6e456f34b36ef1ff24a8974b`;
`app.js` is
`62b414c95a9c4f80db045a38bd95f30a962e609610e79a367ac18fa70ea6040d`.
HTTP read-back hashes match the local package; the FPGA bitstream remains
`1808b9c36da9b7f53ea535e76862d2ae5c2ee2295c336f7964d685d77fdc9129`.
Raw telemetry, build/test logs and the Chrome screenshot are retained in
`tmp/pna-streaming/average-recovery/` (ignored build artifacts).

Chrome verification also caught a network-byte-order error in the new uint64
overrun display. The widget now reads high/low uint32 halves correctly; browser
regressions cover counters above 2^32. The final web bundle was rebuilt and
deployed, and Chrome showed the correct 4-event counter. Before that deployment
the user had selected Y / CIC 80 / 41 averages; those latest settings were
captured and restored. The final browser showed 41/41 with tracking locked.
