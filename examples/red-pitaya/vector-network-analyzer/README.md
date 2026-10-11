# Red Pitaya vector network analyzer

Live forward-transmission analyzer for the 125 MS/s Red Pitaya. It shares the
analyzer workspace, Flot plots, complex thru calibration, reference
controls, sweep planner, DMA capture and browser FFT processor. The board adapter
selects the fixed clock, signed 14-bit converters and physical connector labels.

Connect **OUT1 → device → IN1**. For calibration, remove the device, connect
**OUT1 directly to IN1**, and select **Record thru → Acquire thru**. Reinsert the
device with the same cables and settings, then select **Run sweep**. OUT2 stays
muted; OUT1 returns to zero after each chirp. Source level is digital dBFS and
does not specify calibrated output voltage or power.

The displayed span is **1 kHz–50 MHz**, with 51–2001 points and 20–250 ms chirps.
The broadest span requires at least the 100 ms preset. **Auto** uses 16 Mi samples
for 100 ms chirps (134.2 ms record, 32 MiB download, 7.45 Hz spacing), or 32 Mi
samples for 250 ms (268.4 ms, 64 MiB, 3.73 Hz). **Full** keeps the 64 Mi-sample
record at **536.871 ms**, with **1.86265 Hz** FFT spacing. The browser worker
uses the actual 14-bit DAC quantization when reconstructing the reference.
Downloads and desktop browser processing add to sweep time; the worker uses
approximately 96–384 MiB of memory, caches matching references and downloads
sixteen DMA packets per request. See the shared documentation for record-length
tradeoffs and timing readouts.

Build from the SDK root:

```sh
make doctor validate CFG=examples/red-pitaya/vector-network-analyzer/config.mk
make -j8 CFG=examples/red-pitaya/vector-network-analyzer/config.mk N_CPUS=8
make CFG=examples/red-pitaya/vector-network-analyzer/config.mk HOST=192.168.1.84 run
```

Select `VIVADO_VERSION=2026.1` when using the development host's installed tools.
`make run` streams logs; Ctrl+C leaves the instrument running. The board needs a
V1 OS image with the 128 MiB DMA reservation at `0x18000000` marked `no-map`.
The package is `tmp/examples/red-pitaya/vector-network-analyzer/vector-network-analyzer.zip`.

Open the board's root page for live operation. `?demo=1` explicitly selects
synthetic data. Sessions, calibrations and reference files are kept separate
between boards and between hardware and demo sources.

See the [shared analyzer documentation](../../../web/vector-network-analyzer/README.md)
for controls, calibration matching, exports, acquisition ownership and numerical
tests. Hardware acquisition checks establish that the DMA and browser processing
work; RF accuracy still requires a known loopback, attenuator/filter and cable
delay comparison against an independent instrument.

## Verification

Build checks with Vivado 2026.1 and the SDK GCC 15 ARM builder: routed setup
slack **+0.180235 ns**, hold slack **+0.010958 ns**, and all specified timing
constraints met. Both board packages, the ARM C++23 server, TypeScript assets
and WebAssembly processor built successfully. Shared numerical, RTL, socket
and real Chromium checks are documented in the
[shared VNA README](../../../web/vector-network-analyzer/README.md).
Browser checks cover calibration, references, exports, cancellation, stable
Y axes and workspace geometry at twelve viewport sizes from 320 to 1440 px.

Hardware checks on `192.168.1.84` with its V1 OS:

- Full capture, 128 MiB download and browser processing completed at −30 dBFS,
  returning 2001 points over 1 kHz–50 MHz. Adaptive captures also completed.
- Auto/100 ms returned 1993 distinct bins from 2001 requested points, downloading
  32 MiB. Cached sweeps took approximately **1.02–1.11 s**, with download around
  **0.60 s** and response processing around **0.26–0.36 s**. A fresh worker took
  approximately **1.40 s**. These timings depend on the host and network.
- Repeat sweeps retained plot geometry, Y limits and ticks while traces fit.
  Stationary hover readings matched the latest marker values. Expanding notes
  left both plot rectangles unchanged.
- Cancellation retained the completed response and timing; the next sweep
  succeeded. Saved references and display settings survived installation.
- Batched reads matched the single-block API; invalid counts, out-of-range
  reads and stale/released tokens were rejected. The connected setup clipped
  at −20 dBFS and correctly rejected the sweep.

These are functional acquisition and timing checks. **Hardware thru calibration
and RF accuracy remain unvalidated.** Compare a direct thru, known attenuator/
filter and cable delay against an independent instrument before relying on
numerical RF accuracy. The acquisition driver preserves the boot-time OCM
mappings when enabling its high descriptor bank; replacing the whole mapping
register can hang Linux.

Run the shared numerical and board profile checks with:

```sh
VIVADO_VERSION=2026.1 bash web/vector-network-analyzer/tests/run.sh --red-pitaya
```
