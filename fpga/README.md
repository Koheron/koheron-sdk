# FPGA builds

Run commands from the SDK root. For setup and the full instrument workflow, see
[the SDK README](../README.md) and [development guide](../docs/development.md).

```sh
make CFG=examples/red-pitaya/fft/config.mk ENFORCE_TIMING=1 fpga
make CFG=examples/red-pitaya/fft/config.mk timing
```

`make fpga` builds the selected bitstream. `ENFORCE_TIMING=1` fails the build on
routed timing violations; `make timing` checks routed timing separately.
Use the [board's example](../examples/) for constraints and instrument-specific
validation. Build results and physical acquisition tests must be reported separately.

## Reusing IP synthesis

Global synthesis remains the default. For repeated FPGA builds, experimental
per-IP synthesis and its persistent cache are available through the shared build
flow:

```sh
make CFG=examples/red-pitaya/fft/config.mk VIVADO_VERSION=2026.1 \
  N_CPUS=8 FPGA_SYNTH_MODE=ip ENFORCE_TIMING=1 fpga
```

Vivado caches IP output products under `tmp/ip-cache/<Vivado version>` and reuses
compatible entries across project regeneration and instruments. The first build
can take longer, and synthesis boundaries can change resource use and timing;
keep timing enforcement enabled when evaluating this mode. Switching between
`FPGA_SYNTH_MODE=global` and `ip` regenerates the project automatically.
Changing `MODE`, the Vivado release or installation path, or the FPGA part also
refreshes the affected cached outputs. Enabling `ENFORCE_TIMING=1` checks a
bitstream previously built without timing enforcement before reusing it.
`make clean_fpga` retains the shared cache; remove its version directory to test
a cold cache. See AMD's [synthesis mode documentation](https://docs.amd.com/r/en-US/ug912-vivado-properties/SYNTH_CHECKPOINT_MODE).

ALPHA250-4 PNA is not qualified for per-IP synthesis: it failed setup timing
with Vivado 2026.1 (WNS -0.150 ns). A fresh global build also exposed an existing
timing issue at -0.070 ns, reproduced exactly with the original Vivado scripts
and all 173 implementation-stage checksums identical. The critical path is in
the phase-unwrapper accumulator at 250 MHz.

Cached builds passed routed timing for Red Pitaya FFT/PNA, ALPHA250 FFT/PNA,
ALPHA250-4 FFT and ALPHA15 signal-analyzer with this version. ALPHA250 FFT had
only 0.005 ns of setup margin. Passing on one instrument does not qualify this
mode for every design; no standard instrument enables it by default.

## FPGA rebuild dependencies

Standard instruments on Red Pitaya, ALPHA250, ALPHA250-4 and ALPHA15 select
shared Tcl dependency groups from `fpga/lib/dependencies.mk`. Changes to a PNA
helper therefore do not rebuild FFT or capture designs, while changes to their
common infrastructure still do. Board presets, local Tcl files and timing hooks
are also tracked. Implementation-only hooks listed in `FPGA_IMPL_TCL` rerun
implementation and timing checks while reusing synthesis. Core testbench edits
do not rebuild synthesis packages; packaged documentation remains tracked.
Custom instruments retain the conservative dependency on all `fpga/lib/*.tcl`
files unless they set `FPGA_LIB_TCL` themselves.

When adding a shared Tcl source, add its transitive dependencies to the selected
group, or use `TCL_EXTRA_FILES` for instrument-specific dependencies outside the
board and instrument directories. Keep `TCL_FILES` for a complete override.
Check the rebuild decisions without launching Vivado:

```sh
python3 -m unittest discover -s fpga/tests/build_flow -v
```

Measured Red Pitaya FFT rebuilds with Vivado 2026.1, development mode,
`N_CPUS=8`, and prebuilt core packages on an Intel Core Ultra X7 368H:

| FPGA rebuild | Elapsed |
| --- | --- |
| Global synthesis | 6m33s |
| Per-IP synthesis, empty IP cache | 7m22s |
| Per-IP synthesis, warm IP cache | 4m15s |
| Implementation-hook edit, reusing global synthesis | 3m23s |

All four builds passed the existing routed timing checks. The global build
produced the same FPGA configuration payload as before the build-flow changes;
the hook-only rebuild preserved the synthesis checkpoint's hash and timestamp.

## Source layout

- [cores/](cores/) contains reusable RTL primitives.
- [modules/](modules/) contains Tcl assemblies.
- [ip/](ip/) contains configurable IP subsystems, including the AXI DDS phase modulator.
