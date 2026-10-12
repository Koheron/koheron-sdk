# Koheron SDK V1 release notes

V1 is under development. These notes describe the V1 branch as of 11 October
2026. Boards still ship with V0. Use a V1 OS image for V1 instruments; a V1
image cannot run a V0 instrument. See the
[VCO migration example](MIGRATING.md#worked-example-alpha250-vco) for a minimal
port of an existing instrument, including configuration and C++ changes.

## SDK and deployment

- Instrument builds use `CFG=.../config.mk` instead of `CONFIG=.../config.yml`.
  `memory.yml` defines the hardware and software memory map.
- V1 images load instrument bitstreams and device-tree overlays through Linux
  FPGA Manager. Instrument packages include the FPGA, server and web assets,
  plus board and runtime requirements and the SDK commit.
- `./build.sh <board>` builds a board image with its preset instruments.
  `make run` builds and starts one instrument, while `make deploy` returns after
  the board reports it ready. `make status` and `make logs` provide follow-up
  diagnostics.
- The board management service now runs in C++23. Its web interface and API
  support instrument upload, compatibility checks, activation, boot-default
  selection, status and live logs. Failed activations attempt to restore the
  previous instrument.
- Incremental builds reuse more FPGA, server, web and image work. The SDK Git
  commit is recorded in package metadata without forcing a server recompile.

## Instruments and interfaces

These reference-instrument improvements are separate from the SDK migration
steps above.

- FFT interfaces gained shared spectrum, spectrogram and density views,
  reusable reference captures, CSV/PNG exports and optional persistence.
- The Red Pitaya now has a phase-noise analyzer. Acquisition on ALPHA250 and
  Red Pitaya phase-noise analyzers runs continuously; ALPHA250 and ALPHA250-4
  analyzers support selectable 200/250 MS/s clocks.
- ALPHA250 DPLL controls are available from a live signal diagram.

## Current defaults

- The reference host is Ubuntu 24.04 with Vivado/Vitis 2025.1. `make setup`
  installs the other build dependencies, not the Xilinx tools.
- Images default to Ubuntu Base 26.04.1 with Xilinx Linux and U-Boot 2026.1.
  The default Docker builder uses Ubuntu 26.04 and GCC 15; instrument server
  and board management code use C++23.

See the [development guide](docs/development.md) for instrument commands and
the [OS guide](os/README.md) for image building and installation.
