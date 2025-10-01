
# koheron-sdk

> 🚨 **Breaking change in v1.0**
> **TL;DR:** v1 is not backward-compatible with 0.x.
> - Stay on 0.x use the [`v0-maintenance` branch](../../tree/v0-maintenance)
> - Ready to upgrade: **[MIGRATING.md](./MIGRATING.md)**

Build system for fast development of high-performance instruments on the **Zynq** platform.

---

## Requirements

The SDK is tested on an **Ubuntu 24.04** development machine with **Vivado/Vitis 2025.1**.

```bash
make setup
```

This installs toolchain prerequisites and prepares the workspace.

---

## Getting started

Build the SD card image (Ubuntu 24.04.3 with xilinx-linux-v2025.1 kernel):

```bash
make -j CFG=examples/alpha250/fft/config.mk image
```

Deploy and run the example instrument to a board via the HTTP API:

```bash
make -j CFG=examples/alpha250/fft/config.mk HOST=192.168.1.00 run
```

---

## What the image contains

The image runs **Ubuntu 24.04.3** with the **`xilinx-linux-v2025.1`** kernel.
It is configured with:

- **nginx** serving static web files and proxying **WebSocket** traffic
- An HTTP API via **uWSGI** for uploading / starting / stopping instruments

---

## Instrument package layout

Instruments are packaged as ZIP files containing:

- FPGA **bitstream** and **device-tree overlay**
- **C++ application** (TCP/WebSocket server)
- **Static web** files

Memory regions and other parameters live in a single **YAML** file to keep PL/PS in sync.
Public functions of the C++ application are accessible via **Python (TCP)** or **JavaScript (WebSocket)** clients.

---

## Staying on 0.x

Use branch [`v0-maintenance`](../../tree/v0-maintenance) or tag `v0.24`.
Security/critical fixes only; no new features.

---

## Acknowledgments

This project started as a fork of [red-pitaya-notes](https://github.com/pavel-demin/red-pitaya-notes).
