# ALPHA250 phase-noise analyzer regressions

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
