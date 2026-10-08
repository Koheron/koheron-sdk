# OS image builds

Build and installation: [SDK quick start](../README.md#quick-start).

## Settings

The default runtime is Ubuntu Base **26.04.1** and Xilinx **2026.1 / Linux 6.18**, independent of `VIVADO_VERSION` (default **2025.1**). Bootloader, firmware and device-tree source releases follow the selected Vivado/Vitis toolchain. Keep the development host and build container on Ubuntu 24.04; the board rootfs is a separate environment.

Override `LINUX_VERSION` and `UBUNTU_VERSION` to select earlier releases, for example:

```sh
make CFG=examples/alpha250/fft/config.mk VIVADO_VERSION=2025.1 \
  LINUX_VERSION=2025.1 UBUNTU_VERSION=24.04.5 image
```

APT repositories are derived from the extracted Ubuntu rootfs's `/etc/os-release`. New Xilinx source releases require entries in `source-checksums.sha256`. Defaults refer to released versions, not development snapshots or moving branches.

Rebuild boot and FPGA artifacts when changing Vivado/Vitis versions; cached artifacts are not proof of compatibility. Validate image builds separately from board boot, FPGA overlays, DMA/cache correctness and acquisition tests.

The SDK defconfigs include the full Xilinx 2025.1 → 2026.1 config delta, compared against upstream [`xilinx_zynq_defconfig`](https://github.com/Xilinx/linux-xlnx/blob/xilinx-v2026.1/arch/arm/configs/xilinx_zynq_defconfig) and [`xilinx_defconfig`](https://github.com/Xilinx/linux-xlnx/blob/xilinx-v2026.1/arch/arm64/configs/xilinx_defconfig). Zynq gains explicit ext4, ACL and security-label support and removes redundant scheduler selections. ARM64 gains thermal support, CoreSight/default tracing, NVMe, Type-C, I3C, GPIO aggregation, RPMsg TTY and additional PCIe/PHY support; INA power monitors move from IIO to hwmon, and obsolete selections are removed. Koheron-specific settings are retained. Kconfig dependencies determine which drivers apply to each platform; these selections do not change the Vivado version or runtime kernel default.

Set `PASSWORD` and `TIMEZONE` in the environment before `make image` to override the defaults in [rootfs.mk](./rootfs.mk).

The image manifest records source tags separately from `vivado`, `vivado_build`,
`vitis` and `vitis_build`, queried from `VIVADO_PATH` and `VITIS_PATH` at packaging.
Unavailable values produce a warning and `unknown`. This identifies the selected
tools, not the history of cached artifacts; rebuild artifacts after changing tools.

## Tests

Image-build tests are in [tests/](./tests/); instrument loading tests are in [api/tests/](./api/tests/).

## Management web interface

The OS serves the management pages at `/koheron/`: installed instruments,
running status, server logs, system information and data rates. These pages
live in `os/www/` and share the instrument control styles in
`web/instrument/instrument.css`. Their assets ship with the OS image. The single-page manager
shows installed instruments, logs and system details together. It supports
upload/run/remove feedback, live status updates and log pause, follow and download.

Build them with `make CFG=examples/alpha250/fft/config.mk www`. The output is
`tmp/www/`. Run the host regression suite after installing the dependencies
from `web/package.json`:

```sh
NODE_PATH=web/node_modules node --test os/www/tests/test_*.cjs
```

The suite uses simulated HTTP responses and does not control hardware. Verify
upload, run, removal and log streaming on a board before deploying an OS image.

The data-rate monitor converts server rates from bits/s to bytes/s, uses server
timestamps for its four-minute history, and marks unchanged snapshots as stale.
Monitor tests cover scaling, unit conversions, polling lifecycle and stale data.
