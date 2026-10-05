# ALPHA250 phase-noise analyzer regressions

## Acquisition and spectral processing

From the SDK root:

```sh
bash examples/alpha250/phase-noise-analyzer/tests/run.sh
make CFG=examples/alpha250/phase-noise-analyzer/config.mk server web N_CPUS=4
```

The runner defaults to `.venv/bin/python3`, `cross-armhf:24.04` and
`koheron-web:node20`; override with `PNA_PYTHON`, `PNA_CPP_IMAGE` and
`PNA_WEB_IMAGE`. The Python environment needs NumPy, SciPy and the Koheron
client dependencies. The C++ image needs g++-13 and Eigen; the web image needs
Node.js and npm. If TypeScript or jsdom are missing, the runner caches the
versions specified for these tests under `tmp/tests/alpha250-phase-noise-analyzer/web-deps`.

The production analyzer and DDS are compiled against controlled DMA, MMIO,
clock, ADC and in-memory configuration dependencies. Address and undefined
behavior sanitizers check the execution. The tests cover snapshot access
during a blocked DMA wait, 48-bit DDS rounding with distinct ADC/DAC clocks,
fractional LO configuration round-tripping, large phase-count offsets,
drift rejection and known PM power, settings validation, settling discards,
failed-transfer rejection without a RAM read, rate changes during acquisition,
RF/laser average invalidation and averaging-window growth after unaveraged
acquisition. DMA failures are injected; the DMA hardware driver itself is
not simulated by this fixture.

The shared phase helper is also checked through the existing ALPHA250-4 core
and spectral regressions, including int32 subtraction extremes and near-carrier
detrending response. Python spectra are compared with SciPy's periodogram;
web checks cover exact FFT bin centers, Nyquist, decade density averaging,
linear smoothing with invalid bins, display units, raw/smoothed/PSD export
and existing numeric/DAC controls. No host test establishes analog
noise-floor accuracy. The hardware PM validation below remains necessary.

The tracking test runs the production analyzer and DDS in a simulated loop.
It checks convergence in both directions and channels, correction bounds,
PM power preservation, DMA timing, setting handoff and nominal LO restoration.
One browser regression checks that tracking telemetry preserves nominal LO edits.
The simulation excludes vendor IP, analog effects and real FIFO timing.

The Welch regression compares concurrent phase snapshots with scalar conversion
at signed-count extremes and checks power accumulation bit for bit, including
subnormal values. To exercise ARMv7 NEON and its scalar VFP fallback, use a
cross-compilation image containing `arm-linux-gnueabihf-g++-13`, `qemu-arm`
(the Ubuntu `qemu-user` package) and the ARM sysroot:

```sh
PNA_ARM_IMAGE=your-arm-test-image \
    bash examples/alpha250/phase-noise-analyzer/tests/run-arm.sh
```

This checks numerical equivalence under emulation. Measure throughput on the
board; emulator timing does not establish a hardware performance improvement.

For board tracking validation, first disable tracking and offset the selected
LO above and below the carrier by a small known amount (for example 0.05 Hz).
Enable tracking and check that the implemented LO converges in the correct
direction within the configured bounds, with no change to the other channel.
Repeat on ADC1, toggle tracking off to verify nominal-frequency restoration,
and change CIC rate while tracking. Inject the PM tone described below and
compare integrated tone power with tracking off/on after settling. Check
close-offset spectra separately for effects of the tracking loop and tuning
steps; a converged lock indicator is not a noise-floor calibration.

## FPGA prefilter

From the SDK root:

```sh
PNA_VIVADO_SETTINGS=/tools/Xilinx/2025.1/Vivado/settings64.sh \
    bash examples/alpha250/phase-noise-analyzer/tests/run-fpga.sh
make CFG=examples/alpha250/phase-noise-analyzer/config.mk fpga N_CPUS=4
source /tools/Xilinx/2025.1/Vivado/settings64.sh
vivado -mode batch -source examples/alpha250/phase-noise-analyzer/tests/check_fpga.tcl \
    -tclargs tmp/examples/alpha250/phase-noise-analyzer/fpga/phase-noise-analyzer.xpr
```

The runner reuses the ALPHA250-4 tests for the shared phase-prefilter and LFSR
RTL. An independent convolution of four rectangular kernels checks all four
full-precision sums, signed extremes, impulse response, stochastic rounding,
pipeline delay and reset. Sequence checks cover deterministic seeds, balance
and pair correlations over 100,000 clocks. They do not prove statistical
independence at all lags. The Python test needs NumPy and SciPy and checks unity
gain, mixing-image rejection and known PM through an ideal real mixer/filter/
atan model; vendor IP quantization is excluded.

The block-design check verifies four prefilters, I/Q and rounding-bit ordering,
clock/reset wiring, distinct channel seeds and the independent DAC/reference
paths. The full build enforces routed setup, hold and bus-skew timing.

The integrated design passed Vivado 2025.1 routing at 200 MHz with setup
slack +0.055 ns and hold slack +0.005 ns; the bus-skew constraints also pass.
The shared RTL and ideal mixer regressions, and the generated block-design
connection checks passed. Board validation of this ALPHA250 bitstream remains
pending. The SDK timing report also flags 14 inputs and 41 outputs without
I/O delay constraints; the positive margins certify the constrained paths,
not complete external-interface timing. No internal endpoints are unconstrained.

On a board, connect DAC0 to ADC0 and DAC1 to ADC1. Set each selected local
oscillator and corresponding DAC carrier to 10 MHz, with sinusoidal PM of
1 degree peak at 10 kHz. Check both channel selections at CIC rates 16, 20 and
32 after settling. The integrated tone should give approximately 12.34 mrad
RMS phase jitter, or 196.4 ps RMS time jitter at 10 MHz, if the integration band
contains the tone. Compare PM-on and PM-off spectra. Sweep the modulation
frequency separately to assess passband attenuation. This requires the new
bitstream and does not establish an absolute ADC/DAC noise-floor calibration.

## Moving-average regression

From the repository root, with the server external dependencies installed:

```sh
g++ -std=c++20 -Wall -Wextra -Iserver/external_libs \
    examples/alpha250/phase-noise-analyzer/tests/test_moving_averager.cpp \
    -o /tmp/test_moving_averager
/tmp/test_moving_averager
```

The test compares the actual averager with a chronological queue across growing,
shrinking, partially filled and wrapped windows, repeated resizing, and clearing.
It includes the failing case: append 1 and 2 with capacity 2, grow to 4, append 3;
the average must be 2.

This is a host test. It does not access hardware or test concurrent settings changes.
