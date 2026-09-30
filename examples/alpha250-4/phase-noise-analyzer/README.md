This instrument measures the phase difference between IN0/IN1 (X) or IN2/IN3
(Y), or their cross-spectrum (XY). Channel selectors are X = 0, Y = 1, XY = 2.
The server returns a one-sided phase PSD in rad²/Hz; clients convert it to
single-sideband phase noise with `10 * log10(PSD / 2)`.

Build with:

```sh
make CFG=examples/alpha250-4/phase-noise-analyzer/config.mk
```

When using Vivado 2026.1, add `VIVADO_VERSION=2026.1`.

The FPGA phase multipliers use Q2.30 coefficients normalized to at most unity.
The server restores the result to DUT radians. Rebuild and deploy the FPGA
bitstream and server together: the coefficients are incompatible with the old
integer-multiplier bitstream.

Each accepted spectrum consumes a fresh, non-overlapping DMA window. The XY
average count reports accepted windows, rather than repeated reads of the same
samples. Configuration changes clear the relevant averages and drain queued
samples before accepting new data. Phase-array getters return the most recently
processed, synchronized X/Y snapshot; before the first acquisition they return
zero-filled arrays.

The stitched spectrum has 15001 bins from 30000 input samples. Its frequency
spacing is `fs / 30000`; `get_parameters()` reports the spectrum bin count.
The minimum-frequency setting corresponds to twice that spacing. The underlying
ADC rate is 200 MHz and `fs = 200 MHz / (2 * CIC rate)`.

Run software regressions as described in [tests/README.md](tests/README.md).

Validation on 2026-09-30: ARM server and TypeScript builds passed; software
regressions passed with ASan/UBSan; the Vivado 2026.1 bitstream build completed
for `xc7z020clg400-2`, with routed setup slack +0.156 ns and hold slack +0.025 ns.
No constrained endpoints failed. ADC pin timing, calibration and tracking still
need verification on an ALPHA250-4 board.
