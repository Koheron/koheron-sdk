# Working on Koheron SDK V1

This repository builds instruments for Xilinx Zynq boards: FPGA designs, Linux C++ servers, Python clients and web interfaces. V1 is the default development branch, ahead of the major release. Boards currently ship with V0; V1 development uses a V1 OS image. The `master` branch and `CONFIG=.../config.yml` workflow belong to V0.

## Start with the instrument

* Read the user's instrument goal, board model and any existing project instructions. If the board or intended behavior is missing, ask for that information while inspecting the repository.
* Inspect `git status --short`, the current branch and the SDK commit. Preserve existing user changes and generated build artifacts. Work from the actual checkout rather than assuming it matches an online example.
* Run `make list` to find configurations. Select an example for the exact board; ALPHA250, ALPHA250-4, ALPHA15 and Red Pitaya configurations are not interchangeable.
* Read the selected `config.mk`, `memory.yml`, block design, drivers, clients and any project README or test instructions before editing.
* For a new instrument, copy the nearest example and change `NAME` and `VERSION` in the copy before packaging or deployment. Keep the reference example intact. `NAME` controls archive and runtime identity; it does not rename the C++ command API.

## Build environment

The reference host is Ubuntu 24.04 with Vivado/Vitis 2025.1 at:

```text
/tools/Xilinx/2025.1/Vivado
/tools/Xilinx/2025.1/Vitis
```

Inspect the existing environment with `make doctor CFG=<project>/config.mk`. Use the SDK `.venv/bin/python` for Python tooling and clients. `make setup` installs host dependencies, the Python package, Docker and build images; it does not install Vivado or Vitis. Reuse an existing working setup.

For a different Xilinx installation, pass `VIVADO_PATH` and `VITIS_PATH` on the Make command line consistently. The Makefile assigns these variables, so exporting them in the shell alone does not override the defaults.

Build commands run from the SDK root. Pass `CFG=<project>/config.mk` explicitly on every instrument target. `make validate CFG=<project>/config.mk` checks configuration and memory definitions before a long build.

## Instrument files and generated outputs

* `config.mk`: instrument identity, `BOARD_PATH`, constraints (`XDC`), FPGA cores (`CORES`), C++ sources (`DRIVERS`) and web files (`WEB_FILES`). Use `$(PROJECT_PATH)` for files in the instrument and `$(SDK_PATH)` for shared SDK files.
* `memory.yml`: memory regions, registers, Linux device mappings and parameters. It generates Tcl, C++ and device-tree definitions; keep hardware and software interfaces synchronized here.
* `block_design.tcl`: FPGA composition and connections. Reusable SDK RTL cores are under `fpga/cores/`.
* C++ driver public methods: the Python and TypeScript command API. Update clients and relevant protocol tests when signatures or response shapes change.
* `OVERRIDE_DTSI`: selects an instrument-specific device-tree override when needed. V1 loads bitstreams through FPGA Manager and device-tree overlays.
* `WEB_FILES`: adds assets to the shared SDK defaults. Archive contents are flat, so asset basenames must be unique.

With the default `TMP=tmp`, generated files are under `tmp/<project>/`. Edit source files, not generated Tcl, headers, metadata or built web assets. For `examples/alpha250/fft/config.mk`:

```text
tmp/examples/alpha250/fft/fft.zip           # instrument archive
tmp/alpha250/instruments/fft.zip           # copy used by image builds
tmp/examples/alpha250/fft/alpha250-fft.zip  # OS image release archive
```

Derive these paths from the selected project, `NAME`, board and `TMP`; do not hard-code ALPHA250 FFT paths into another instrument.

## Validate and build

Choose checks relevant to the changed layer, then build the complete archive:

```bash
make validate CFG=<project>/config.mk
make -j CFG=<project>/config.mk server  # C++ and generated command metadata
make -j CFG=<project>/config.mk web     # web assets
make -j CFG=<project>/config.mk fpga    # FPGA bitstream
make -j CFG=<project>/config.mk        # complete instrument archive
```

Run the existing project tests for the affected behavior. Test entrypoints are documented beside the instrument, for example in `examples/alpha250/fft/tests/README.md`. Tests that connect to a board require the intended board and setup; do not treat all `test.py` files as host-only unit tests.

After changing clocks, constraints or FPGA logic, run `make CFG=<project>/config.mk timing`. Set `ENFORCE_TIMING := 1` in the instrument configuration to enforce routed timing checks during FPGA builds; without it, builds report timing failures as warnings. Review warnings about incomplete I/O constraints separately.

Use incremental builds. Do not use `clean_all` as a first response to a failure: it removes the entire output tree. Investigate the failing layer and use project-scoped cleaning when needed.

## Deploy to the intended board

Use the board address supplied for the task; never rely on the Makefile's default `HOST`. Check whether that board is already running V1. Changing the SDK branch does not change the board OS.

`make CFG=<project>/config.mk HOST=<board-ip> run` rebuilds, uploads and starts the instrument, then streams logs indefinitely. Pressing `Ctrl+C` ends the local log stream and leaves the instrument running. Do not wait for this command to exit as proof of successful deployment.

For unattended deployment, use the SDK Python API's `upload_instrument(host, archive, run=True)`, then `instrument_status(host)` and a client command that verifies the requested behavior. Read the runtime and client logs when startup or a command fails. The selected instrument name alone does not prove correct hardware operation.

Bound network checks with a command timeout: the Python HTTP helpers currently do not set request timeouts.

Rebuild the OS with `make -j CFG=<project>/config.mk image` for initial V1 setup or changes to the OS, kernel, boot files, services or image defaults. Normal FPGA, C++ and web changes use the instrument archive. Image creation does not write an SD card. If the task includes writing a card, identify the exact target device first; `make flash` can discover multiple matching card readers when no device is specified.

## Report the result

State what changed, the selected board/configuration, SDK commit, output archive, checks run and their results. Distinguish configuration validation, compilation, routed timing and board measurements. If tools or hardware are unavailable, report the exact missing prerequisite and the checks completed.

The current GitHub configuration-check workflow validates example configurations; a green result does not establish that FPGA builds or hardware tests passed.
