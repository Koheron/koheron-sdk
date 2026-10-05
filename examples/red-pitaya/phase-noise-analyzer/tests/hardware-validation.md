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
