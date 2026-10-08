# PNA NEON critical-path optimization

Validated on 2026-10-08 against V1 commit `bf41e191` using the Red Pitaya at
`192.168.1.84`. These changes affect the shared C++ phase-noise processing path.

## Implementation

The window preparation, normalized power reduction and three-periodogram mean
now collect safety flags in NEON registers and reduce them once per output.
This removes repeated scalar/NEON transfers and branches on Cortex-A9. Unsafe
windows or spectra still use the original scalar calculations before returning;
the rolling mean repairs exceptional blocks. Rare exceptional inputs can cost
an additional pass. Numerical scaling and gradual-underflow behavior are retained.

The raw integer trend fit remains a separate function. In the production ARM
LTO build, inlining it into the larger FFT caller caused accumulator spills
inside its sample loop and erased the reduction savings. Its vector loop uses
an explicit multiple-of-four bound, retaining the scalar tail for other sizes.

## Build and regression checks

- GCC 13 ARM server builds with the repository's existing optimization and
  warning flags: Red Pitaya PNA, ALPHA250 PNA, ALPHA250-4 PNA and ALPHA250 DPLL.
- `PNA_TEST_MODE=docker bash server/drivers/phase-noise/tests/run-host.sh all cpp`:
  address/undefined-behavior sanitizer tests and independent numerical audits.
- The shared streaming Welch regression compiled with the production ARMv7
  NEON flags and executed on the Red Pitaya itself. Its generated spectra passed
  `check_streaming_welch.py` against SciPy; the largest relative error was below
  0.85 ppm, within the existing 2 ppm tolerance.
- Added regression coverage for late exceptional power bins, subnormal rolling
  auto spectra and signed cross spectra. Normalization and rolling results match
  scalar calculations bit for bit, treating either sign of zero as equivalent.

No FPGA implementation was needed for these C++ changes. ALPHA250, ALPHA250-4
and DPLL hardware were not tested in this run.

## Red Pitaya hardware measurements

The baseline and candidate were built from the same source revision with only
these C++ edits differing. Only `serverd` was temporarily replaced; the FPGA
payload and driver RPC schema matched throughout. ADC0 used the existing DAC0
loopback, 10 MHz carrier, +8 phase precision, 32768-point FFTs and an averaging
target of 91. Tracking was disabled. Complete 16385-bin spectrum snapshots were
requested about 60 times per second, with performance counters sampled every
200 ms after warmup.

The sustained comparison used four 20-second intervals in baseline, candidate,
candidate, baseline order, with equivalent restarts and settings. Results below
average the two interval medians per version; service time excludes waiting for
samples. Measurements depend on the current board and client load.

| Measurement | Baseline | Candidate | Reduction |
| --- | ---: | ---: | ---: |
| Total service (ms) | 10.706 | 10.301 | 3.8% |
| FFT worker (ms) | 7.009 | 6.776 | 3.3% |
| Spectrum reduction / rolling mean (ms) | 1.621 | 1.369 | 15.5% |
| Process CPU (% of one core) | 94.382 | 92.189 | 2.3% |

Every sustained CIC 50 interval retained full unique-sample coverage and had no
new ring overruns, phase-overflow captures, DMA errors or gap captures. Spectrum
snapshots stayed valid, finite and nonnegative. The remaining largest FFT stage
is the transform itself, around 3.5 ms.

At CIC 20, separate eight-second intervals processed about 97 accepted captures
per second on the baseline and 100 on the candidate. Both remain CPU limited
against the required 190.7 segments/s and incur ring overruns at this rate; this
change does not establish continuous lossless acquisition at CIC 20.

A 0.1 rad sine PM tone at 2441.40625 Hz, CIC 50 and averaging target 8 produced
0.00500718 rad² on the baseline and 0.00500658 rad² on the candidate against
0.005 rad² expected (+0.14% and +0.13%). The test waited 32 accepted captures
after each stimulus change to settle the overlapping estimator and rolling
average. The shorter wait was unreliable after an overloaded-rate run.

The original server binary, FPGA hash, analyzer settings, nominal LOs, precision
and native DAC configuration words were restored and checked. Reversible test
scripts, raw counter samples, captures and build logs are under the local ignored
directory `tmp/pna-critical-paths/`.
