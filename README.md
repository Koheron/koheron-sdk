# koheron-sdk

Build instruments for Xilinx Zynq boards: FPGA designs, Linux images, C++ servers and web interfaces.

> **V1 is the default branch and recommended for new instrument development.** It remains under development ahead of the major release.
> Boards ship with V0; use a V1 OS image for V1 development. V1 images do not support V0 instruments.
> See [MIGRATING.md](./MIGRATING.md) for porting or [Staying on 0.x](#staying-on-0x) for the supplied image.

## Requirements

Reference host: **Ubuntu 24.04** with **Vivado/Vitis 2025.1** under `/tools/Xilinx/2025.1`. The SDK targets Vivado/Vitis **2025.1 and newer**; select the installed release with `VIVADO_VERSION=...`. Override `VIVADO_PATH` and `VITIS_PATH` on the Make command line for other installation paths.

Install Vivado/Vitis and any required board files or licenses separately. `make setup` installs host dependencies, the Python environment and Koheron package, Docker and SDK Docker images.

Server, instrument and board management builds use C++23 with GCC 15 in Docker.
The standalone C++ client continues to support C++20.

## Quick start

```bash
git clone https://github.com/Koheron/koheron-sdk.git
cd koheron-sdk
make setup

# Build an ALPHA250 image with the FFT instrument
make -j CFG=examples/alpha250/fft/config.mk image
```

Extract the `.img` from `tmp/examples/alpha250/fft/alpha250-fft.zip` and write it to an SD card. Writing erases the card; keep the supplied V0 card to return to V0.

Insert the card with the board powered off, then boot and find its IP address. The image includes the FFT instrument. Use an example for your board when setting `CFG`.

Build and deploy subsequent instrument changes:

```bash
make -j CFG=examples/alpha250/fft/config.mk HOST=192.168.1.100 run
```

`run` streams logs after starting the instrument. `Ctrl+C` stops the stream and leaves it running. The [Python upload/run API](https://www.koheron.com/software-development-kit/documentation/v1/python-api/#upload-and-run-an-instrument) returns after deployment for scripts and agents. Rebuild the image for OS, kernel, boot, board support or default instrument changes.

The default Docker builder uses GCC 15 for the server, Linux kernel and
U-Boot and the board management runtime. See [compiler settings](docker/README.md)
for independent selections. The reference host remains Ubuntu 24.04.

## Configuration model

`CFG` selects the instrument's `config.mk`:

- `config.mk`: name, board, FPGA cores, constraints, drivers and web assets.
- `memory.yml`: memory map, registers, Linux mappings and parameters; generates Tcl, C++ and device-tree definitions.

`SDK_PATH` is the SDK root; `PROJECT_PATH` is the directory containing `config.mk`.

## Development workflow

Pass `CFG=.../config.mk` to build and deployment targets. `make help` lists targets; `VERBOSE=1` adds build details.

| Command | Description |
| --- | --- |
| `make` or `make all` | Builds the FPGA bitstream, server, web assets and packages them into an instrument ZIP. |
| `make fpga` | Generates the Vivado bitstream defined in the selected `config.mk`. |
| `make server` | Compiles the C++ TCP/WebSocket server. |
| `make web` | Builds the TypeScript/CSS assets for the web UI. |
| `make os` | Builds the Linux root filesystem for the selected board. |
| `make image` | Produces a bootable SD card image combining OS, boot files and instrument artefacts. |
| `make run` | Builds, uploads and starts the instrument, then streams logs. |
| `make copy CFG=... DEST=...` | Copies an instrument's sources and sets its package name from the destination directory. |
| `make doctor` | Checks host tools and an optional `CFG`. |
| `make list` | Lists example instruments. |
| `make validate CFG=...` | Validates `config.mk` and `memory.yml`. |

Pull requests and pushes run a one-minute smoke-test job for example
configurations, instrument copying, OS build dependencies and C++ logging.
A separate Docker job builds and tests the native C++23 management runtime,
installer and LED helper with GCC 15. Full server, sanitizer,
instrument build, DSP and browser tests remain available to run locally when a
change needs them.

### Reusing IP synthesis

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

### FPGA rebuild dependencies

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

## Creating a new instrument

```bash
make copy CFG=examples/alpha250/fft/config.mk DEST=examples/alpha250/my-instrument
```

`copy` sets `NAME := my-instrument`; the archive and stored instrument use this name. It refuses an existing destination or the source name. Metadata and dependency caches are skipped.

Board settings, `VERSION`, driver names and shared SDK references are preserved. The API class remains `FFT`. Change hard-coded client instrument names, such as `connect(host, 'fft')`, to `my-instrument`.

```bash
make validate CFG=examples/alpha250/my-instrument/config.mk
make -j CFG=examples/alpha250/my-instrument/config.mk
```

## Repository layout

```text
boards/    # Board definitions, boot components and helper makefiles
docker/    # Dockerfiles used for reproducible builds
examples/  # Reference instruments with ready-to-use config.mk and memory.yml files
fpga/      # Common FPGA build logic (make fragments, Tcl helpers)
os/        # Linux image build system and board-specific settings
python/    # Python tooling, runners and client libraries
server/    # C++ server sources and build rules
web/       # Front-end assets shared across instruments
```

Within `fpga/`, `cores/` contains reusable RTL primitives, `modules/` contains
Tcl assemblies, and [`ip/`](./fpga/ip/) contains configurable Vivado IP
subsystems, starting with the AXI DDS phase modulator.

## Instrument packaging

Instrument ZIP: `tmp/<project>/<NAME>.zip`, also copied to `tmp/<board>/instruments/<NAME>.zip`. Contents:

- Runtime FPGA bitstream binary loaded by FPGA Manager (`.bit.bin`).
- Device-tree overlay (`pl.dtbo`).
- Original Vivado bitstream kept for debugging/reference (`.bit`).
- Compiled server executable (`serverd`).
- Driver metadata (`drivers.json`).
- Built web assets referenced by the server.
- A `version` file tying the artefacts together.
- `instrument.json` identifying the board, architecture and runtime requirements
  for deployment preflight.

## Image contents

Generated SD card images boot **Ubuntu Base 26.04.1** with the **Xilinx 2026.1 (Linux 6.18)** kernel. OS defaults are pinned independently of the selected Vivado/Vitis release; boot components and device-tree generation follow that toolchain. The runtime environment includes:

- **nginx** serving static files and proxying **WebSocket** traffic.
- A **C++23 HTTP API**, native installer and LED helper for instrument management.

The standard board image contains no Python interpreter, Flask or uWSGI.
The host Python SDK continues to use the same HTTP routes.

See [OS image build notes](./os/README.md) for settings and tests.

## Staying on 0.x

Use the [`master` branch](https://github.com/Koheron/koheron-sdk/tree/master) with the V0 image supplied with your board:

```bash
git clone -b master https://github.com/Koheron/koheron-sdk.git
```

Follow the [V0 documentation](https://www.koheron.com/software-development-kit/documentation/) and use `CONFIG=.../config.yml`. [V0 images](https://www.koheron.com/software-development-kit/documentation/ubuntu-zynq/) remain available. For a fixed SDK revision, use a published 0.x release such as [V0.24](https://github.com/Koheron/koheron-sdk/releases/tag/V0.24).

## Further resources

- [AGENTS.md](./AGENTS.md) — short SDK hints for coding agents.
- [MIGRATING.md](./MIGRATING.md) — guidance for upgrading existing instruments to V1.
- [boards/](./boards) — board definitions and bootloader settings.
- [examples/](./examples) — complete reference designs you can adapt for your projects.

## Acknowledgments

This project started as a fork of [red-pitaya-notes](https://github.com/pavel-demin/red-pitaya-notes).
