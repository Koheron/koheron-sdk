# ALPHA250 vector network analyzer

Separate chirp-based forward-transmission instrument at 250 MS/s. It uses the
[shared VNA workspace and acquisition components](../../../web/vector-network-analyzer/README.md)
with the SDK's FFT/PNA styling, complex thru calibration, phase/group delay,
markers, references and exports. The existing DMA examples remain independent.

Connect **OUT0 → device → IN0**. For calibration, connect **OUT0 directly to IN0**
with the measurement cables and select **Record thru → Acquire thru**. Insert the
device using the same cables and settings, then select **Run sweep**. OUT1 stays
muted; OUT0 returns to zero after each chirp.

The displayed span is **1 kHz–100 MHz**. Excitation extends from half the start
frequency to 1.1 times the stop frequency. Choose 51–2001 logarithmic points,
20–250 ms chirps and −40–0 dBFS digital source level. The full span supports the
100/250 ms presets; narrower spans allow shorter chirps. **Auto** uses 32 Mi
samples for 100 ms chirps (134.2 ms record, 64 MiB download, 7.45 Hz spacing),
or 64 Mi samples for 250 ms. **Full** keeps a 64 Mi-sample record with
**3.72529 Hz** FFT spacing and **268.435 ms** duration; downloads
and desktop browser processing add to sweep time.

The clock policy selects the internal TCXO reference and 250 MS/s, holds the
shared sampling lock while starting/checking a capture and rejects a changed
clock. The profile uses signed 16-bit ADC/DAC values without a source shift.

## Build and deployment

From the SDK root, select the installed toolchain:

```sh
make validate doctor CFG=examples/alpha250/vector-network-analyzer/config.mk VIVADO_VERSION=2026.1
make -j8 CFG=examples/alpha250/vector-network-analyzer/config.mk VIVADO_VERSION=2026.1 N_CPUS=8
make CFG=examples/alpha250/vector-network-analyzer/config.mk VIVADO_VERSION=2026.1 HOST=192.168.1.100 run
```

The package is `tmp/examples/alpha250/vector-network-analyzer/vector-network-analyzer.zip`.
The standard SDK build produces the FPGA bitstream, checked timing reports,
ARM server, device-tree overlay, browser assets and WebAssembly processor.
`make run` streams logs; Ctrl+C leaves the instrument running. Open the board's
root page for live operation; `?demo=1` explicitly selects synthetic data.

The board requires a compatible **V1 OS image**. Its fixed 128 MiB DMA range
at `0x18000000–0x1fffffff` must be reserved exclusively with `no-map` at boot.
The server rejects overlap with Linux System RAM. An instrument ZIP cannot
add that reservation to an old boot image. See
[DMA reservation](../../../os/tests/dma-reservation/README.md).

## Verification

Build checks with Vivado 2026.1 and the SDK GCC 15 ARM builder: routed setup
slack **+0.030921 ns**, hold slack **+0.047078 ns**, and all eight bus-skew
constraints passed. Setup margin is narrow. Inherited board constraints leave
14 inputs and 25 outputs without delay constraints. ARM C++23, TypeScript,
WebAssembly and instrument configuration checks passed.

Shared numerical, RTL and real Chromium checks are documented in the
[shared VNA README](../../../web/vector-network-analyzer/README.md).

**ALPHA250 hardware validation remains to be performed.** Check a direct thru,
known attenuator/filter, cable delay, repeatability and level dependence against
an independent instrument before relying on numerical RF accuracy.
