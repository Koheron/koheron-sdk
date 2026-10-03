# koheron-sdk

Build instruments for Xilinx Zynq boards: FPGA designs, Linux images, C++ servers and web interfaces.

> **V1 is the default branch and recommended for new instrument development.** It remains under development ahead of the major release.
> Boards ship with V0; use a V1 OS image for V1 development. V1 images do not support V0 instruments.
> See [MIGRATING.md](./MIGRATING.md) for porting or [Staying on 0.x](#staying-on-0x) for the supplied image.

## Requirements

Reference host: **Ubuntu 24.04** with **Vivado/Vitis 2025.1** under `/tools/Xilinx/2025.1`. Override `VIVADO_PATH` and `VITIS_PATH` on the Make command line for other installation paths.

Install Vivado/Vitis and any required board files or licenses separately. `make setup` installs host dependencies, the Python environment and Koheron package, Docker and SDK Docker images.

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

## Image contents

Generated SD card images boot **Ubuntu 24.04.5** with the **`xilinx-linux-v2025.1`** kernel. The runtime environment includes:

- **nginx** serving static files and proxying **WebSocket** traffic.
- An HTTP API (powered by **uWSGI**) for uploads and instrument management.

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
