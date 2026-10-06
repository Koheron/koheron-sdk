# Red Pitaya continuous DMA validation

Validated on 2026-10-05 at `192.168.1.84`, with DAC0 connected to ADC0 (LV).
Sources: `V1` at `75a34120`, plus the unchanged-control and cached-zero-spectrum
fixes on `fix/redp-pna-runtime-validation`. Instrument version: 1.2.0.
Analyzer and both DAC settings were saved before deployment and restored after
each hardware test. The final server, FPGA binary and browser bundle were read
back over HTTP and matched the built archive.

## Build and software checks

- Full instrument build with Vivado 2025.1 and GCC 13, `N_CPUS=4`.
- Routed WNS +0.459450 ns, WHS +0.019423 ns; 15 bus-skew constraints checked.
- 12954 LUTs, 20306 registers, 45.5 block RAM tiles and 63 DSPs.
- Block-design assertions passed for the shared phase extractor, selected
  ADC/reference muxes, cyclic SG DMA, resets and packet metadata.
- Shared acquisition, tracking, publication, DMA and numerical regressions
  passed, including sanitizer checks. All 48 browser regressions passed.

## Continuous acquisition and controls

With CIC 20, 25 averages and +8 bits, a 15-second run alongside the browser
received 59.2 spectrum snapshots/s, observed 35.8 changed publications/s and
counted 37.6 accepted captures/s. DMA advanced 5727 packets without changing
the acquisition epoch or increasing sample-gap, DMA-error or overflow counters.
The browser reported approximately 36 fresh spectra/s with a 60 reads/s target.
These are different rates; cached reads do not count as fresh spectra.

With matched CIC 20 / 25-average / standard-precision / PM-off settings and the
same browser plus 60-read/s TCP workload, the previously installed 1.1.0
instrument accepted 23.0 captures/s. The upgraded instrument accepted
39.5 captures/s, with 38.0 changed publications/s and 59.3 reads/s. This is an
upgrade comparison, not a throughput gain attributed to the unchanged-control
fix. Zooming into the dense 200 kHz–1 MHz noise floor remained live at
approximately 39 FPS, with about 1.1 ms drawing time averaged over replies.
Reference capture/clear retained the zoom, and Fit restored the full view.

Changed settings recovered at CIC 4, 20, 67 and 8192. Reapplying the existing
channel, rate, precision, LO and average target retained the DMA epoch and
averaging history. At CIC 8192, that batch took 1.4 ms; a changed rate took
29.1 seconds to produce its first valid capture. Rapid combinations of rate,
channel and precision changes returned in 7.9–11.7 ms and recovered with the
requested metadata. ADC1 was exercised for switching, without an ADC1 loopback
calibration test.

At slow decimation, standard precision can produce a valid all-zero density
after small fluctuations are quantized away. The UI now skips repainting an
unchanged zero spectrum, while retaining zoom/resize and readiness recovery.

## PM calibration and remaining small-signal error

The test integrates bins 62–66 after subtracting an unmodulated baseline.
The sine PM tone is at bin 64; expected power is `beta² / 2` rad².

| CIC | Extra bits | Peak PM | Power error |
| --- | --- | --- | --- |
| 20 | 0 | 100 mrad | +0.13% |
| 20 | 4 | 100 mrad | +0.10% |
| 20 | 8 | 100 mrad | +0.12% |
| 67 | 8 | 100 mrad | +0.11% |
| 4 | 8 | 100 mrad | +0.11% |
| 20 | 8 | 10 mrad | +0.19% |

At CIC 20 and +8 bits, five 1 mrad tests at a 10 MHz carrier measured
6.8–11.0% below the expected power, failing the 5% calibration criterion.
With 100 averages, the errors were −8.20% at 10 MHz, −2.78% at
10,000,000.637 Hz, and −1.67% at 12,345,678 Hz. More averages do not remove
the 10 MHz bias. The responsible stage in the DAC/ADC/analyzer path remains
unisolated; these results do not establish 1 mrad accuracy or a calibrated
noise floor. No empirical correction was applied.

The calibration script now compares the sample rate with its float32 wire
representation, allowing CIC 67, and selects RF mode before restoring the
previous analyzer mode.

Raw build logs, settings snapshots, timing reports and measurements are in
`tmp/pna-redp-review/`; PM spectra are in
`tmp/tests/red-pitaya-phase-noise-analyzer/`.

## Follow-up: shared Cartesian precision (2026-10-06)

The shared multiplier, I/Q prefilter and CORDIC inputs now retain 24 bits on
Red Pitaya, ALPHA250 and ALPHA250-4. The prefilter retains all intermediate
sum bits. Carrier-power telemetry drops the extra eight bits after filtering,
preserving its original units. The shared phase quantizer separates rounding
and saturation into four pipeline stages; stream throughput and packet format
stay unchanged.

AMD's installed bit-accurate multiplier/CORDIC models, compared with full-product
integer-input filter/atan2 references, isolate a digital rounding defect:

| Sample clock | ADC peak (14-bit codes) | Worst 16-bit I/Q power error | Worst 24-bit I/Q power error |
| --- | --- | --- | --- |
| 125 MS/s | 2000 | 18.22% | 0.0695% |
| 125 MS/s | 4800 | 7.12% | 0.0368% |
| 200 MS/s | 2000 | 18.51% | 0.1019% |
| 200 MS/s | 4800 | 9.92% | 0.0438% |
| 250 MS/s | 2000 | 17.38% | 0.0641% |
| 250 MS/s | 4800 | 6.87% | 0.0244% |

Each row sweeps thirteen carrier phases at 1 mrad peak PM. This establishes a
digital-stage improvement, not analog calibration accuracy. RTL checks pass
for both 16- and 24-bit prefilters, rounding, range guards, acquisition epochs,
stalls and packet metadata. Connection checks pass on all three generated
board designs.

The 24-bit Red Pitaya loopback still fails the 5% weak-PM criterion. At
10 MHz, CIC 20, +8 bits and 100 averages, carrier phases 0°, 30°, 60°, 90°,
120° and 150° measured power errors of −7.49%, +57.15%, −24.19%, −26.55%,
−1.64% and −3.54%. At 100 mrad, errors remain within 0.13% at CIC 4, 20 and
67; the 10 mrad / CIC 20 result is −0.05%.

An actual packaged AWG RTL simulation at the same six phases measures digital
stimulus power errors between −2.38% and +0.14%, including Red Pitaya's DAC
bit truncation. Changing only the receiver LO by 0–50 Hz leaves the loopback
bias near −8%. The tests rule out those digital stages as an explanation of
the large carrier-phase-dependent loopback error; they do not separate analog
DAC errors from ADC errors. That distinction needs an independent calibrated
source. No empirical gain correction or relaxed hardware tolerance is applied.

Follow-up model results are in `tmp/tests/pna-cartesian-precision/summary.json`;
loopback spectra and measurements are in `tmp/pna-redp-review/final-spectra/`
and `calibration-final.json`. The generator simulation and analysis are in
`tmp/pna-redp-review/weak-model/`.

### Final builds and deployment

All three complete instrument archives pass the strict Vivado 2025.1 setup,
hold, pulse-width and bus-skew gates with the shared 24-bit Cartesian path and
four-stage phase quantizer:

| Board | Setup slack (ns) | Hold slack (ns) | Bus-skew constraints |
| --- | --- | --- | --- |
| Red Pitaya | +0.421511 | +0.007958 | 15 |
| ALPHA250 | +0.076660 | +0.040732 | 10 |
| ALPHA250-4 | +0.046937 | +0.012307 | 12 |

The Red Pitaya DAC mailbox initially failed its existing 4 ns bus-skew limit
by 0.178 ns despite positive setup/hold margins. The shared PNA post-route hook
reroutes mailbox nets only when their skew fails, then repeats the hold fix;
the final SDK timing gate remains unchanged. The final Red Pitaya build needed
one retry. The other boards needed none. Inherited external I/O-delay omissions
remain; these results certify the constrained paths.

The complete ALPHA250-4 software suite passes, including sanitizers and a new
production-driver regression proving that unchanged controls preserve averages,
publication and DMA epochs. Reapplying the nominal LO after a tracking correction
still retunes. ALPHA250 and ALPHA250-4 have build, RTL, model and connection
validation for this change; this revision has not been tested on those boards.

Red Pitaya's final archive was deployed at `192.168.1.84`. The server, browser
bundle and FPGA binary read back over HTTP match the archive. FPGA SHA-256:
`3b410f559d8c595223f7174399c329cbe4d6480e08884f8fc4255f11aff588b1`.
Archive SHA-256:
`160aabb3c4616e4e915043682dfb936b93b0b7f3676bff594e2111d9291e49d0`.
The calibration results above are from this final image. The test deliberately
fails its unchanged 5% assertion for weak PM and restores analyzer/DAC settings
in its cleanup path; the failed criterion is not waived.

A final 10-second check at CIC 20, +8 bits and 25 averages served 59.15 snapshots/s,
observed 37.37 changed publications/s and accepted 38.97 captures/s. DMA advanced
3821 packets with the same epoch and no new gaps, DMA errors or overflows.
RPC median/p95 latency was 2.89/13.17 ms. The browser showed Connected,
25/25 averages and Live precision, with approximately 34 fresh spectra/s at a
60 reads/s target. Build logs, deployment readback and the acquisition benchmark
are in `tmp/pna-redp-review/build-final-*.log`, `deploy-final.log` and
`final-cic20-bits8.json`.

## Welch native-order power optimization (2026-10-06)

The shared Red Pitaya/ALPHA250 estimator now uses unordered PFFFT transforms,
accumulates power in compact native order and maps the merged sum directly
into the published PSD. The plan's permutation is derived with PFFFT at
construction; there is no per-segment complex-spectrum reorder. FFT lengths,
Hann weights, overlap, accumulation order and normalization are retained.
ALPHA250-4's stitched periodograms do not repeat Welch segments, so this
optimization does not change that estimator.

### Numerical and build validation

- Independent ordered-FFT references match every PSD bin bit for bit for
  one, two, three and five segments, including signed raw-count extremes.
- Host SIMD and scalar backends pass ASan/UBSan; the NEON/VFP regression passes
  directly on Red Pitaya, including subnormal power, DC/Nyquist packing,
  nonfinite values and concurrent phase snapshots.
- Acquisition, tracking, DMA, publication and numerical regressions pass;
  all 48 browser regressions pass.
- Strict ARM server builds and instrument packaging pass for Red Pitaya and
  ALPHA250. ALPHA250 hardware validation remains pending. The deployed Red
  Pitaya FPGA, overlay, RPC metadata and browser assets are byte-identical
  to the previous archive; this is a server update using that routed image.

### Isolated ARM and live measurements

With the receiver service paused, three runs alternate the old and new
estimators over the same 65536-sample signed-count ramp plus small PM. Each
run measures 80 calls per variant, including window preparation, three FFTs,
power accumulation and phase snapshot conversion; the drift fit is outside
the timed region. All 16385 PSD bins match bit for bit.

| Run | Previous median (ms) | Native-order median (ms) |
| --- | --- | --- |
| 1 | 15.3678 | 14.7653 |
| 2 | 16.0449 | 15.0331 |
| 3 | 15.8560 | 14.9647 |

These runs show a 3.9–6.3% reduction in estimator time on Cortex-A9. Host SSE
measurements were approximately neutral and do not predict the ARM gain.

Separate 12-second live runs use ADC0, +8 bits, 25 averages, tracking off,
matched 10 MHz LO/carrier and 0.74° peak sine PM at 10 kHz. The client targets
60 snapshot reads/s and samples processing telemetry at 4 Hz:

| CIC | Accepted captures/s, previous → native | Median processing ms, previous → native |
| --- | --- | --- |
| 4 | 42.40 → 44.29 | 21.41 → 20.24 |
| 20 | 42.82 → 44.91 | 21.21 → 20.66 |

Each run retains its DMA epoch and reports no new gaps, DMA errors or
overflows. These are short controlled comparisons, not maximum throughput
guarantees. Separate +8-bit loopback checks at CIC 20 recover 100 mrad and
10 mrad peak PM with +0.12% and +0.09% power error respectively. The earlier
1 mrad carrier-phase-dependent calibration limitation remains unresolved;
this optimization does not alter the measured estimator values.

The new Red Pitaya server was deployed and read back over HTTP:
`serverd` SHA-256
`8dd8abf539ad202ad5e8d2f1c2557760fe41e85c105cd852b21beda2790d5fe1`.
Archive SHA-256
`4a062f6a8b7a8fb15b983fa971469607e2ec3d63ae3aaba9f6c4a9f7a862178a`.
Saved live analyzer and native DAC settings were restored after every hardware
test. The browser shows Connected and Live precision at approximately 41 fresh
spectra/s with the user's restored controls. Artifacts and logs are in
`tmp/pna-native-order/`: `benchmark-arm.log`, `arm-regression-final.log`,
`kernel-checks-final.log`, `regressions-final.log`, `build-*-server-final.log`,
`before-live.json`, `after-live.json`, `deploy.log` and `calibration-*.log`.

## GCC ARMv7 PFFFT memory-access optimization (2026-10-06)

Keep GCC 13 and the existing `-O3`, ARMv7 hard-float and NEON settings.
Use explicit `vld1q_f32`/`vst1q_f32` memory accesses in the real forward
radix-2/4 butterflies, complex radix-2/3/4/5 butterflies and FFT finalization.
Arithmetic, twiddles, FFT sizes, output packing and scaling are unchanged.
Scalar, host SIMD and other compiler backends retain typed accesses.
All three PNAs share this implementation, including ALPHA250-4's 30000-point
complex transform. No FPGA or browser changes are involved.

The original GCC build uses paired 64-bit VFP loads/stores for ordinary NEON
vector dereferences. In the isolated `radf4_ps` object, explicit accesses reduce
78 `vldr`/`vstr` instructions to zero; `vld1`/`vst1` instructions change from
30 to 61. Alignment-only changes did not improve code generation. Global
Cortex-A9 tuning helped the complex FFT but slowed the real FFT; stage
specialization enlarged code without a consistent benefit. Neither is shipped.

### Numerical and build checks

- Compare the final core with the unmodified core from `f84fe179`, using two
  separately prefixed copies in one GCC ARM executable. Across 21 real/complex
  sizes, forward/backward transforms, native/canonical order and in-place or
  separate buffers, all 12,239,360 output comparisons match. Finite values
  are bit-identical; NaNs retain their classification. Inputs cover zero,
  impulses, DC, random data, tiny/large values and nonfinite values.
- Buffers use PFFFT's allocator; the ARM comparison also tests 8-byte-aligned
  buffers offset from its 64-byte allocations. No alignment requirement is
  strengthened by the optimization.
- The committed transform regression passes independent double DFT, Parseval,
  ordering and in-place forward/inverse checks with host SIMD/scalar sanitizers
  and directly on Red Pitaya NEON. Existing ARM Welch tests also pass.
- Full acquisition, tracking, DDS, averaging, publication, cyclic DMA and
  ALPHA250-4 numerical tests pass; all 48 browser regressions pass.
- Strict GCC ARM server builds pass for Red Pitaya, ALPHA250 and ALPHA250-4.
  The wrapper's system-header pragma omitted vendor dependencies from `-MMD`,
  so the server build now tracks the PFFFT C sources and headers explicitly.
  Dry-run header changes recompile PFFFT and relink each of the three servers.

### Isolated Cortex-A9 measurements

Stop the instrument server during isolated measurements, then restore it and
the saved analyzer/DAC settings. Use GCC 13 with `-O3 -fno-math-errno
-march=armv7-a -mfpu=neon -mfloat-abi=hard -mvectorize-with-neon-quad`.
Plans and aligned buffers are cached. Measure native forward transforms with
four calls per timed block, alternating the two cores. Drop ten warm-up rounds
and report the median of 90 blocks. Setup and reorder are outside the timing;
the corresponding canonical outputs match bit for bit.

| Repeat | Real 32768 before / after (ms) | Complex 30000 before / after (ms) |
| --- | --- | --- |
| 1 | 3.1274 / 3.0144 | 8.0710 / 7.2856 |
| 2 | 3.1058 / 2.9761 | 8.1410 / 7.3531 |
| 3 | 3.2205 / 3.0262 | 8.0937 / 7.2987 |

This is 3.6–6.0% less real FFT time and 9.7–9.8% less complex FFT time.
Separate full-estimator runs include window preparation, worker launch,
ordered output where needed, PSD calculation and phase snapshot conversion.
All 16385 Welch PSD bins and 15001 complex cross-density bins are bit-identical.
The two-channel 30000-point cross-density calculation improves from
14.83–15.01 ms to 13.68–13.76 ms (7.2–8.9%). Welch medians vary more: two
runs improve by 3.8–4.6%, while one regresses by 1.4%; the raw FFT saving
does not translate into an identical full-estimator speedup on every run.
ALPHA board hardware measurements remain pending; these kernel measurements
use Red Pitaya's Cortex-A9 with the shared ALPHA250-4 estimator.

### Live acquisition and deployment

Matched 12-second runs use ADC0, +8 fractional bits, 25 averages, tracking off,
10 MHz carrier/LO, 10 kHz PM at 0.74 degrees, 60 snapshot reads/s and
precision telemetry at approximately 4 Hz. The browser is disconnected during
both runs so its polling does not change the workload.

| CIC | Previous captures/s | Optimized captures/s | Processing median before / after (ms) |
| --- | --- | --- | --- |
| 4 | 44.07 | 45.29 | 19.982 / 19.962 |
| 20 | 44.45 | 45.70 | 20.348 / 19.727 |

No new DMA gaps, errors, overflows or acquisition epoch changes occur within
either run. These are short controlled comparisons, not guaranteed maximum
rates. The live acquisition improvement is approximately 2.8%.

Replace only `serverd` in each existing instrument archive, preserving every
other member byte for byte. Upload and run the Red Pitaya archive, then check
HTTP readback against the package. The deployed server SHA256 is
`9389aaf3f6a685eb5cc3f25a63f3436dcddf9d5b357756b087f6e25e550a4a2c`;
the archive SHA256 is
`1acd21bdbe5703d0d9ee46c114da501dece3895cff155c84727c18f57b73cff7`.
The FPGA, app.js and RPC metadata retain the hashes in the preceding validation.
Restore the freshly saved controls after all tests, including the user's current
50-average target and the native DAC words. The optimized instrument is running;
the browser shows Connected, Live precision, 50/50 averages and approximately
40 displayed FPS with those restored settings.

Artifacts are in `tmp/pffft-gcc/`: `core-bench-final.log`, `pna-bench.log`,
`arm-final-checks.log`, `transform-tests.log`, `regressions.log`,
`build-*-final.log`, `dependency-check.json`, `before-live.json`,
`after-live.json`, `deploy.log`, settings snapshots and before/after archives.

## Single CIC restored in 1.3.1 (2026-10-06)

The split-CIC 1.3.0 FPGA introduced hardware sample gaps on this 125 MHz
board. Twenty-four trials across both inputs, CIC 10, 20, 50 and 80, and three
acquisition restarts per setting recorded 447 gaps. The previous 1.2.0 FPGA
recorded zero in the same trials, using either the previous server or the new
streaming server. This isolates the regression to the new FPGA acquisition
path; it does not identify the failing RTL stage.

Version 1.3.1 restores the six-stage programmable Xilinx CIC on the ADC clock,
followed by the compensation FIR and quantizer. The filtered stream crosses
to FCLK1 through the existing DMA FIFO. Integer CIC rates, including odd rates,
are supported again. The 65536-sample phase snapshots, cyclic SG DMA ring,
packet metadata, precision controls and new streaming DSP remain in use.
ALPHA250 instruments retain their split filter.

### Build checks

- Full Vivado 2025.1 instrument build, ARM server, browser bundle, overlay and
  ZIP package passed with timing enforcement enabled.
- Routed setup slack +0.284088 ns, hold slack +0.008955 ns; pulse-width checks
  and 15 bus-skew constraints passed. Inherited external I/O-delay omissions
  remain; these margins cover the constrained paths.
- Red Pitaya connection assertions passed for the single CIC, 125 MHz
  filter clocks, configuration handshake, epoch resets, precision/gap metadata,
  phase extraction, ADC/reference muxes and cyclic DMA.
- Seven numeric-control browser tests passed, including odd-rate Red Pitaya
  acceptance and even-rate ALPHA250 validation. ALPHA250 project generation
  and split-filter connection assertions also passed. This follow-up did not
  repeat ALPHA250 routing or hardware measurements.

### Hardware tests

The complete 1.3.1 archive was deployed to `192.168.1.84`. HTTP readback of
the server, FPGA binary, overlay, RPC metadata and browser bundle matched the
archive. FPGA SHA-256:
`914bdf5870c99ebbc68aa348ed29636304d03d7348b254caf3c3cda6bee2bead`.
Archive SHA-256:
`0ad63e1c7218597aa8f1017e086987b9a118b6781d0e8417c64b4fe7a6e2d50a`.

Thirty-six two-second trials covered both inputs, CIC 4, 10, 20, 50, 67 and
80, with three precision-triggered acquisition restarts per setting. All
trials produced valid captures: 5137 accepted in total, with zero hardware
sample gaps, DMA errors or overflows. The CPU can still skip processing hops
at fast rates; that coverage counter is separate from hardware sample gaps.

At CIC 8192, parameter and spectrum reads returned in 0.76 and 2.50 ms while
acquisition was settling. Changing to odd rate 67 recovered valid spectra.
This checks control responsiveness and cancellation, not a complete slow-rate
capture at 8192.

A separate 60.06-second run used the restored ADC1, CIC 43, +8-bit precision,
70 averages and 10 MHz LOs. It accepted 4872 captures and advanced DMA by
10654 packets without new gaps, DMA errors, overflows or epoch changes.
The phase snapshot contained 65536 finite float32 radians; spectra contained
16385 finite float32 density bins with positive non-DC power. All polled
spectra stayed valid, with 1624 distinct publications observed. Snapshot RPC
median/p95 latency was 2.82/3.35 ms. These measurements are finite-duration
acquisition tests, not a new PM gain calibration or a maximum-rate guarantee.

Fresh analyzer and native DAC settings were saved before deployment and
matched the final readback after all tests. No server ERROR/CRITICAL entries
occurred after the new FPGA loaded. The board remains running 1.3.1.

Evidence is in `tmp/review/`: `redp-single-cic-build.log`,
`redp-single-cic-connections.log`, `redp-single-cic-web-test.log`,
`alpha-split-connections.log`, `redp-deployment-single-cic.json`,
`gap-sweep-single-cic.json`, `redp-single-cic-rate-transition.json`,
`redp-runtime-single-cic.json` and the settings/readback snapshots.


## Split-CIC gap investigation: reset decoding (2026-10-06)

The split design's gaps are caused by its reset path in the tested Red Pitaya
implementation. They are not an inherent limitation of two unrelated 125 MHz
clocks. The production AXIS converter has `IS_ACLK_ASYNC=1`; it was not
accidentally configured as a synchronous crossing.

### Hardware comparison

First, the original split FPGA was kept unchanged while a diagnostic server
changed FCLK1 to 125, 100, 111.111 and back to 125 MHz. The ADC clock remained
125 MHz. For each clock, both inputs were tested at CIC 10, 20, 50 and 80 with
three one-second acquisition restarts per setting. Gap counters increased by
447, 648, 407 and 481 respectively, with no DMA errors. A different clock ratio
alone did not cure the failure. These counters count gap detections and retries,
not the number of missing ADC instants.

A second diagnostic FPGA included both the original decoded reset and a
registered reset, selected by a stable control bit. The filter, converter,
DMA, server, placement and routing were identical between modes. Two 125 MHz
passes per mode reproduced the failure and recovery when switching modes;
one pass per mode also exercised each slower clock:

| FCLK1 | Trials per mode | Decoded-reset gap detections | Registered-reset gap detections | Registered-reset accepted captures |
| --- | ---: | ---: | ---: | ---: |
| 125 MHz | 48 | 1296 | 0 | 3420 |
| 111.111 MHz | 24 | 650 | 0 | 1716 |
| 100 MHz | 24 | 647 | 0 | 1693 |

Every registered-reset trial accepted captures. All 192 trials had zero new
DMA errors; all 96 registered-reset trials also had zero raw hardware-gap
polls. The decoded-reset mode accepted no captures in this particular routed
image. That differs from the original split image's intermittent success,
consistent with the failure depending on implementation delays.

A separate 60.014-second registered-reset run at 125 MHz used ADC1, CIC 80,
+8 bits, 70 averages and 10 MHz LOs. It accepted 2862 captures, advanced DMA
by 5724 packets and published 1432 distinct spectra, with no new gaps, DMA
errors, overflows or epoch changes. Phase and PSD snapshots contained 65536
and 16385 finite float32 values respectively. Snapshot median/p95 latency
was 2.56/4.06 ms. This is acquisition validation, not a PM calibration.

After the diagnostic test, the known-good single-CIC 1.3.1 image and the saved
analyzer/native DAC settings were restored and verified. FCLK1 returned to
125 MHz. The single-CIC Red Pitaya topology remains the production choice.

### Mechanism and regression checks

`filter_resetn` previously decoded CONFIGURE, PRIME and STREAM from a binary
state register. PRIME (`011`) to STREAM (`100`) changes three bits. Unequal
physical delays can briefly make the decoded reset low. The ADC-clock logic
may miss this pulse while `phase_stream_cdc`'s asynchronous reset synchronizer
asserts the destination reset. AMD documents that behavior for
[XPM_CDC_ASYNC_RST](https://docs.amd.com/r/en-US/pg382-xpm-cdc-generator/XPM_CDC_ASYNC_RST).

A controlled post-route timing experiment added 400 ps to one state-bit path.
The decoded reset produced a 395 ps pulse and asserted the destination reset
in 10/10 epochs; the registered reset produced no pulses in 10/10 epochs.
These are controlled SDF experiments, not measurements of the board's pulse
width. The same-image hardware comparison above isolates the reset-path
change, independently of this chosen simulation skew.

The shared single-stream controller and ALPHA250-4 paired controller now drive
reset directly from an ADC-clock register, retaining the 32-clock reset hold.
The hazard is common to the split designs; its presence does not depend on
whether the ADC clock is 125 or 250 MHz. The new synthesis regression verifies
a direct register-Q reset source for both rate steps and the paired controller;
it rejects the previous LUT decoder. Existing controller simulations pass
14463 single-stream samples and 9607 matched paired samples over seven epochs.

Separate clock-matrix RTL simulations use the production converter and FIR.
Each board model passes 102 epochs across clock phases and small frequency
offsets, including forced backpressure and recovery. Red Pitaya covers
125-to-125 and 125-to-100 MHz; ALPHA250 covers 250-to-142.857 MHz. These RTL
checks establish functional clock-crossing behavior, not absence of physical
combinational glitches or hardware validation of the 250 MHz boards.

### Build checks

The switchable diagnostic FPGA passed strict setup, hold, pulse-width and all
16 bus-skew checks: WNS +0.017005 ns, WHS +0.009009 ns. An isolated
registered-reset split build initially failed setup timing in the DAC control
and DMA paths. Reimplementation with `Performance_Explore` passed the same
unchanged gates: WNS +0.079575 ns, WHS +0.021342 ns, 16 bus-skew checks.
Only timing-qualified images were deployed. Neither diagnostic clock/reset
RPC nor the mode selector is part of the production sources.

Evidence is in `tmp/review/`: `clock-hardware-sweep.json`,
`reset-mode-hardware-sweep.json`, `redp-runtime-reset-select.json`,
`redp-reset-select-build.log`, `redp-registered-reset-explore.log`,
`control-timing-manual/simulate-skew.log`,
`control-timing-fixed-manual/simulate.log`, `clock-matrix-redp.log`,
`clock-matrix-alpha.log`, and deployment/settings readbacks.

## Final production image with registered reset (2026-10-06)

The final single-CIC 1.3.1 production package includes the shared controller's
registered reset. It was built with Vivado 2025.1 and passed strict setup,
hold, pulse-width and 15 bus-skew checks: WNS +0.178767 ns and WHS +0.013317 ns.
The ARM server, web bundle, overlay and instrument package also build.

The deployed archive SHA-256 is
`a19df30ecd5e0706a00ab339f5e417acbcab167c19a45078b44b70e6f4dcb1f0`;
the FPGA binary SHA-256 is
`ae00331cf7f34822c075b9d21c8dc975bff12d7498b25d10210df347f8b47982`.
Readback verified the deployed file hashes on `192.168.1.84`.

Hardware tests covered both inputs, CIC 4, 10, 20, 50, 67 and 80, and three
two-second restarts per setting. All 36 trials accepted captures: 5062 total,
with zero new hardware gap detections, DMA errors or overflows. CPU coverage
and skipped processing windows are separate from hardware sample gaps.

A separate 60.035-second run at CIC 50 and +8-bit precision accepted 4581
captures, advanced DMA by 9160 packets and observed 1527 distinct spectra.
There were no new hardware gaps, DMA errors, overflows or epoch changes.
Snapshots contained 65536 finite float32 phase values and 16385 finite
float32 PSD bins. Snapshot median/p95 latency was 2.819/3.325 ms.
These are finite-duration acquisition checks, not a PM calibration or a
maximum-rate guarantee.

Analyzer and native DAC settings matched the saved pre-deployment readback
after testing. There were no server ERROR/CRITICAL entries after FPGA load.
The board remains running this production image.

The final ALPHA250 and ALPHA250-4 PNA packages also pass routed timing;
hardware testing on those boards is pending. The final DPLL build fails
setup by 0.031810 ns in its gain-programming command-enable path and remains
unqualified. See the [build results](../../../../fpga/lib/pna_filter.md#final-registered-reset-build-checks--2026-10-06).

Evidence is in `tmp/review/`: `redp-final-registered-reset-build.log`,
`redp-deployment-registered-final.json`, `gap-sweep-registered-final.json`,
`redp-runtime-registered-final.json`, `redp-registered-final-validation.log`,
`alpha250-final-registered-reset-build.log`,
`alpha250-4-final-registered-reset-build.log` and
`dpll-final-registered-reset-build.log`.
