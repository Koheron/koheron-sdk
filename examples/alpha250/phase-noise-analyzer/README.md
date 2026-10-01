# ALPHA250 phase-noise analyzer

## Phase conversion

The server converts filtered DMA counts to radians before returning `get_phase()`
or computing phase-noise PSD and jitter. Clients must not apply another phase
calibration to those outputs.

The CORDIC/unwrapper scale is `pi / 8192` radians per unfiltered count. For the
current six-stage CIC with differential delay 1 and 32-bit input/output, the
low-frequency filter correction at CIC rate `R` is:

```text
C(R) = 4 * 2^ceil(log2(R^6)) / R^6
phase_radians = filtered_DMA_counts * C(R) * pi / 8192
```

The factor 4 compensates the FIR's fixed-point scaling: 32 fractional coefficient
bits, a 66-bit accumulator, and a 32-bit output give a DC gain of approximately
1/4. The second factor compensates the CIC's power-of-two truncation of its
full-precision gain `R^6`. See [AMD PG140](https://docs.amd.com/r/en-US/pg140-cic-compiler/Output-Width-and-Gain)
and [PG149](https://docs.amd.com/r/en-US/pg149-fir-compiler/Output-Width-and-Bit-Growth).

At rate 20, the correction is 4.194304, replacing the former fixed 4.196 value
(about -0.0404% in phase amplitude and -0.00351 dB in phase PSD). At power-of-two
rates the correction is 4. Phase PSD scales with the square of this correction;
phase and time jitter scale linearly. Rate changes and phase processing share
the acquisition mutex, and the existing two-transfer settling discard is retained.

This correction assumes the current CIC/FIR configuration and concerns gain near
DC. It does not compensate passband frequency response or the optical delay-line
transfer function. The fixed-point widths were checked with isolated IP generated
in Vivado 2026.1; verification with a known electrical phase modulation on hardware
remains necessary. See [issue #711](https://github.com/Koheron/koheron-sdk/issues/711).

## Validation

Run the host regression from the repository root (Python 3 and a C++20 compiler):

```sh
CXX=g++-13 python3 examples/alpha250/phase-noise-analyzer/tests/test_phase_calibration.py
make CFG=examples/alpha250/phase-noise-analyzer/config.mk server N_CPUS=2
```

The regression checks every supported rate against exact integer CIC gain,
including the maximum rate, power-of-two rates, and truncation-bit boundaries.

On a board, apply a known low-frequency electrical phase modulation, compare
phase amplitude at rates 16, 20, and 32, and switch rates during acquisition.
Check that settled phase, PSD, and jitter agree with the expected modulation.
Sweep modulation frequency separately to assess the measurement passband.
