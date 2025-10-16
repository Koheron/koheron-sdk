
# koheron-sdk

Build high-performance instruments for Xilinx Zynq-based boards with a single toolchain that drives FPGA, embedded Linux, C++ servers and web front-ends.

> **Breaking change in v1.0**
> **TL;DR:** v1 is not backward-compatible with 0.x.
> - Staying on 0.x? Use the [`v0-maintenance` branch](../../tree/v0-maintenance).
> - Ready to upgrade? Follow **[MIGRATING.md](./MIGRATING.md)**.

---

## Table of contents

1. [Features](#features)
2. [Requirements](#requirements)
3. [Quick start](#quick-start)
4. [Development workflow](#development-workflow)
5. [Repository layout](#repository-layout)
6. [Instrument packaging](#instrument-packaging)
7. [Image contents](#image-contents)
8. [Staying on 0.x](#staying-on-0x)
9. [Further resources](#further-resources)
10. [Acknowledgments](#acknowledgments)

---

## Features

- Unified `make` flow to build FPGA bitstreams, Linux images, TCP/WebSocket servers and web interfaces from one configuration file.
- Optimized for rapid iteration on Zynq-7000 and Zynq UltraScale+ instruments.
- Generates deployable instrument archives that can be pushed to boards over HTTP.
- Supports per-project Vivado block designs, memory maps and driver customisation via modular makefiles.

---

## Requirements

The SDK is developed and tested on **Ubuntu 24.04** with **Vivado/Vitis 2025.1** installed in `/tools/Xilinx`.

Run the helper target to create a Python virtual environment, install host dependencies and prepare the workspace:

```bash
make setup
```

Additional board-specific dependencies (Vivado board files, licenses, etc.) should be installed before launching the build.

---

## Quick start

```bash
git clone https://github.com/Koheron/koheron-sdk.git
cd koheron-sdk
make setup

# Build an example instrument image
make -j CFG=examples/alpha250/fft/config.mk image

# Deploy the instrument to a board via HTTP
make -j CFG=examples/alpha250/fft/config.mk HOST=192.168.1.100 run
```

Replace `CFG` with the path to another `config.mk` to target a different instrument or board.

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

## Repository layout

```
boards/    # Board definitions, boot components and helper makefiles
docker/    # Dockerfiles used for reproducible builds
examples/  # Reference instruments with ready-to-use config.mk files
fpga/      # Common FPGA build logic (make fragments, Tcl helpers)
os/        # Linux image build system and board-specific settings
python/    # Python tooling, runners and client libraries
server/    # C++ server sources and build rules
web/       # Front-end assets shared across instruments
tests/     # Automated tests for the SDK and example instruments
```

Exploring these directories is the best way to learn how to assemble your own instrument configuration.

---

## Instrument packaging

Running `make` (with `CFG` set) produces `<instrument>.zip` in `tmp/<board>/instruments/`. Each archive contains:

- FPGA **bitstream** (`.bit`) and **device-tree overlay** (`pl.dtbo`).
- Boot-time **bitstream binary** (`.bit.bin`).
- Compiled **server executable** (`serverd`).
- Built **web assets** referenced by the server.
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

If you rely on the 0.x toolchain, use the [`v0-maintenance`](../../tree/v0-maintenance) branch or the `v0.24` tag. Only security and critical bug fixes are backported.

---

## Further resources

- [MIGRATING.md](./MIGRATING.md) — guidance for upgrading existing instruments to v1.
- [boards/](./boards) — board definitions and bootloader settings.
- [examples/](./examples) — complete reference designs you can adapt for your projects.

---

## Acknowledgments

This project started as a fork of [red-pitaya-notes](https://github.com/pavel-demin/red-pitaya-notes).
