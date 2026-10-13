# Koheron SDK

Build FPGA instruments, Linux images, C++ servers and web interfaces for Xilinx
Zynq boards.

**V1 is the default branch and remains under development.** Boards ship with V0.
Use a V1 OS image for V1 instruments; it does not run V0 instruments. See
[migration](MIGRATING.md) or [staying on 0.x](#staying-on-0x).

## Requirements

Reference host: **Ubuntu 24.04** with **Vivado/Vitis 2025.1**, including board
files and licenses. Install Xilinx tools separately. `make setup` installs the
remaining host dependencies, Python environment, Docker and builder images.
For newer Vivado/Vitis releases, set `VIVADO_VERSION=...`.

## Quick start

From the SDK root, build a V1 ALPHA250 image with FFT as the default instrument
and the board's other preset instruments:

```sh
make setup
./build.sh alpha250
```

Use `./build.sh --list` for other boards.

See [OS images](os/README.md#flashing-an-existing-image) for installing the
image. Boot the board and open `http://<board-ip>/` to use FFT.

## Working on instruments

`CFG` selects the instrument; its `memory.yml` defines the hardware and software
memory map. To update the instrument on a running V1 board, set `HOST` to its IP
address or hostname:

```sh
make -j CFG=examples/alpha250/fft/config.mk HOST=192.168.1.100 run
```

`run` builds and installs the instrument, then streams logs. Ctrl+C ends the
stream; the instrument keeps running. Rebuild the image for OS, kernel, boot,
board support or default instrument changes.

## Documentation

- [V0 to V1 SDK changelog](RELEASE_NOTES.md): detailed changes and migration impact.
- [Release validation](VALIDATION.md): checked revisions, results and remaining release criteria.
- [Development guide](docs/development.md): commands, configuration and tests.
- [FPGA builds](fpga/README.md): bitstream and timing commands.
- [OS images](os/README.md): image builds and installation.
- [Board management](os/api/README.md): runtime API, status, logs and web interface.
- [STEMlab 125-14 board variants](boards/red-pitaya/README.md): original and Gen 2 targets.
- [Build compilers](docker/README.md): Docker builders and compiler settings.

## Staying on 0.x

Use the [`master` branch](https://github.com/Koheron/koheron-sdk/tree/master)
with the V0 image supplied with your board. Follow the
[V0 documentation](https://www.koheron.com/software-development-kit/documentation/)
and use `CONFIG=.../config.yml`. [V0 images](https://www.koheron.com/software-development-kit/documentation/ubuntu-zynq/)
remain available; [V0.24](https://github.com/Koheron/koheron-sdk/releases/tag/V0.24)
provides a fixed SDK revision.

## Acknowledgments

This project started as a fork of
[red-pitaya-notes](https://github.com/pavel-demin/red-pitaya-notes).
