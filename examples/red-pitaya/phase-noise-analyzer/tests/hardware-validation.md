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
