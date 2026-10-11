# Koheron SDK

Build instruments for Xilinx Zynq boards: FPGA designs, Linux images, C++ servers
and web interfaces.

**V1 is the default branch for new development and remains under development.**
Boards ship with V0; use a V1 OS image for V1 instruments. V1 images do not
support V0 instruments. See [migration](MIGRATING.md) or [staying on 0.x](#staying-on-0x).

## Requirements

Reference host: **Ubuntu 24.04** with **Vivado/Vitis 2025.1** installed separately,
including required board files and licenses. Newer releases are supported;
select the installed release with `VIVADO_VERSION=...`. `make setup` installs
host dependencies, the Python environment, Docker and SDK builder images.
C++23 builds use GCC 15 in Docker. See [compiler settings](docker/README.md).

## Quick start

Use an [example for your board](examples/) when selecting `CFG`. This builds an
ALPHA250 image with the FFT instrument:

```sh
git clone https://github.com/Koheron/koheron-sdk.git
cd koheron-sdk
make setup
make -j CFG=examples/alpha250/fft/config.mk image
```

Insert an SD card and replace `/dev/sdX` below with its whole-disk device.
Flashing erases the card; keep the supplied V0 card if you need to return to V0.

```sh
sudo --preserve-env=PATH make CFG=examples/alpha250/fft/config.mk FLASH_DEVICE=/dev/sdX flash
```

`flash` writes the existing release ZIP without rebuilding it. Boot the board
from the new card, find its IP address, then build and deploy instrument changes:

```sh
make -j CFG=examples/alpha250/fft/config.mk HOST=192.168.1.100 run
```

`run` streams logs over WebSocket with HTTP fallback. Ctrl+C closes the stream
and leaves the instrument running. Use `make deploy` to return after confirmed
readiness. Rebuild the image for OS, kernel, boot, board support or default instrument changes.

## Development workflow

`CFG=.../config.mk` selects the instrument; its `memory.yml` defines the hardware
and software memory map. Start with these local commands:

```sh
make help
make list
make doctor TARGET=web CFG=examples/alpha250/fft/config.mk
make info CFG=examples/alpha250/fft/config.mk FORMAT=json
make check
```

`make check` runs host regressions; `make test CFG=... HOST=...` runs the
instrument's hardware test. `make status HOST=...` reads board state and
`make logs HOST=...` returns recent logs.

See the [development guide](docs/development.md) for the Make command reference,
deployment timeouts, JSON output, new instruments and package contents.

## Documentation

- [Development guide](docs/development.md): configuration, commands, deployment and tests.
- [FPGA builds](fpga/README.md): timing, synthesis caching, rebuild dependencies and measurements.
- [OS images](os/README.md): board image builds, boot components and the management runtime.
- [Build compilers](docker/README.md): Docker builders and compiler overrides.
- [Migration](MIGRATING.md): porting instruments from 0.x to V1.
- [Agent instructions](AGENTS.md): short working conventions for coding agents.

Instrument guides live with their [examples](examples/); board support lives in
[boards/](boards/).

## Staying on 0.x

Use the [`master` branch](https://github.com/Koheron/koheron-sdk/tree/master)
with the V0 image supplied with your board:

```sh
git clone -b master https://github.com/Koheron/koheron-sdk.git
```

Follow the [V0 documentation](https://www.koheron.com/software-development-kit/documentation/)
and use `CONFIG=.../config.yml`. [V0 images](https://www.koheron.com/software-development-kit/documentation/ubuntu-zynq/)
remain available; [V0.24](https://github.com/Koheron/koheron-sdk/releases/tag/V0.24)
provides a fixed SDK revision.

## Acknowledgments

This project started as a fork of
[red-pitaya-notes](https://github.com/pavel-demin/red-pitaya-notes).
