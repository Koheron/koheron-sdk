# koheron-sdk

Build high-performance instruments for Xilinx Zynq-based boards with a Make-based toolchain that coordinates FPGA, embedded Linux, C++ servers and web front-ends.

> **Breaking change in V1**
> **TL;DR:** V1 is not backward-compatible with 0.x.
> - Starting a V1 project? Clone and build the `V1` branch explicitly.
> - Upgrading from 0.x? Follow **[MIGRATING.md](./MIGRATING.md)**.

---

## Table of contents

1. [Features](#features)
2. [Requirements](#requirements)
3. [Quick start](#quick-start)
4. [Configuration model](#configuration-model)
5. [Development workflow](#development-workflow)
6. [Creating a new instrument](#creating-a-new-instrument)
7. [Repository layout](#repository-layout)
8. [Instrument packaging](#instrument-packaging)
9. [Image contents](#image-contents)
10. [Staying on 0.x](#staying-on-0x)
11. [Further resources](#further-resources)
12. [Acknowledgments](#acknowledgments)

---

## Features

- Unified `make` flow to build FPGA bitstreams, Linux images, TCP/WebSocket servers and web interfaces.
- Optimized for rapid iteration on Zynq-7000 and Zynq UltraScale+ instruments.
- Generates deployable instrument archives that can be pushed to boards over HTTP.
- Supports per-project Vivado block designs, memory maps and driver customisation via modular makefiles.

---

## Requirements

The SDK is developed and tested on **Ubuntu 24.04** with **Vivado/Vitis 2025.1** installed in `/tools/Xilinx`.

Run the helper target to prepare the host:

```bash
make setup
```

`make setup` installs host dependencies, creates `.venv`, installs the Koheron Python package, prepares Docker, and builds the SDK Docker images. You may need to log out and back in before Docker commands work without `sudo`.

Additional board-specific dependencies (Vivado board files, licenses, etc.) should be installed before launching the build.

---

## Quick start

```bash
git clone -b V1 https://github.com/Koheron/koheron-sdk.git
cd koheron-sdk
make setup

# Build an example instrument archive
make -j CFG=examples/alpha250/fft/config.mk

# Build a bootable SD card image
make -j CFG=examples/alpha250/fft/config.mk image

# Deploy the instrument to a board via HTTP
make -j CFG=examples/alpha250/fft/config.mk HOST=192.168.1.100 run
```

Replace `CFG` with the path to another `config.mk` to target a different instrument or board.

---

## Configuration model

V1 no longer uses the old `CONFIG=.../config.yml` flow. Each instrument is selected with `CFG=.../config.mk`:

- `config.mk` contains build settings such as the instrument name, board path, Vivado cores, drivers and web assets.
- `memory.yml` lives next to `config.mk` and defines the memory map, registers, Linux devices and build-time parameters used to generate FPGA, C++ and device-tree artefacts.

For example, `examples/alpha250/fft/config.mk` selects the Alpha250 board, FFT drivers and web files, while `examples/alpha250/fft/memory.yml` defines register regions, `/dev/mem_wc` mappings and parameters such as `fft_size`.

---

## Development workflow

Common targets provided by the top-level `Makefile`:

| Command | Description |
| --- | --- |
| `make` or `make all` | Builds the FPGA bitstream, server, web assets and packages them into an instrument ZIP. |
| `make fpga` | Generates the Vivado bitstream defined in the selected `config.mk`. |
| `make server` | Compiles the C++ TCP/WebSocket server. |
| `make web` | Builds the TypeScript/CSS assets for the web UI. |
| `make os` | Builds the Linux root filesystem for the selected board. |
| `make image` | Produces a bootable SD card image combining OS, boot files and instrument artefacts. |
| `make run` | Uploads and starts the instrument on a remote board through the HTTP API. |

Verbose logs are available by passing `VERBOSE=1`, and the active board/instrument configuration is controlled through the `CFG` variable.

---

## Creating a new instrument

Start by copying a nearby example, then edit the build settings, memory map, drivers and web UI for your hardware design.

```text
examples/<board>/<instrument>/
  config.mk
  memory.yml
  block_design.tcl
  <driver>.hpp
  <driver>.cpp
  web/
```

For example:

```bash
cp -r examples/alpha250/fft examples/alpha250/my-instrument
make -j CFG=examples/alpha250/my-instrument/config.mk
```

---

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

Exploring these directories is the best way to learn how to assemble your own instrument configuration.

---

## Instrument packaging

Running `make` with `CFG` set produces `<instrument>.zip` in `tmp/<board>/instruments/`. Each archive contains:

- Runtime FPGA bitstream binary loaded by FPGA Manager (`.bit.bin`).
- Device-tree overlay (`pl.dtbo`).
- Original Vivado bitstream kept for debugging/reference (`.bit`).
- Compiled server executable (`serverd`).
- Driver JSON generated from the selected drivers.
- Built web assets referenced by the server.
- A `version` file tying the artefacts together.

The instrument archive can be uploaded with `make run` or the HTTP API directly, and is consumable by the Python client utilities located in [`python/`](./python).

---

## Image contents

Generated SD card images boot **Ubuntu 24.04.3** with the **`xilinx-linux-v2025.1`** kernel. The runtime environment includes:

- **nginx** serving static files and proxying **WebSocket** traffic.
- An HTTP API (powered by **uWSGI**) to upload, start and stop instruments.

This setup lets you iterate rapidly without having to rebuild the entire OS for every code change.

---

## Staying on 0.x

If you rely on the 0.x toolchain, use a published 0.x release such as **V0.24** from the GitHub releases page. A dedicated `v0-maintenance` branch is not referenced here until it is published.

---

## Further resources

- [MIGRATING.md](./MIGRATING.md) — guidance for upgrading existing instruments to V1.
- [boards/](./boards) — board definitions and bootloader settings.
- [examples/](./examples) — complete reference designs you can adapt for your projects.

---

## Acknowledgments

This project started as a fork of [red-pitaya-notes](https://github.com/pavel-demin/red-pitaya-notes).
