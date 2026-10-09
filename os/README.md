# OS image builds

Build and installation: [SDK quick start](../README.md#quick-start).

## Settings

The default runtime is Ubuntu Base **26.04.1** and Xilinx **2026.1 / Linux 6.18**, independent of `VIVADO_VERSION` (default **2025.1**). Bootloader, firmware and device-tree source releases follow the selected Vivado/Vitis toolchain. The reference development host remains Ubuntu 24.04; the default build container uses Ubuntu 26.04 and GCC 15.

Override `LINUX_VERSION` and `UBUNTU_VERSION` to select earlier releases, for example:

```sh
make CFG=examples/alpha250/fft/config.mk VIVADO_VERSION=2025.1 \
  LINUX_VERSION=2025.1 UBUNTU_VERSION=24.04.5 GCC_VERSION=13 image
```

Build the [Ubuntu 24.04/GCC 13 fallback container](../docker/README.md) before using this older runtime configuration.

APT repositories are derived from the extracted Ubuntu rootfs's `/etc/os-release`. New Xilinx source releases require entries in `source-checksums.sha256`. Defaults refer to released versions, not development snapshots or moving branches.

Rebuild boot and FPGA artifacts when changing Vivado/Vitis versions; cached artifacts are not proof of compatibility. Validate image builds separately from board boot, FPGA overlays, DMA/cache correctness and acquisition tests.

The SDK defconfigs include the full Xilinx 2025.1 → 2026.1 config delta, compared against upstream [`xilinx_zynq_defconfig`](https://github.com/Xilinx/linux-xlnx/blob/xilinx-v2026.1/arch/arm/configs/xilinx_zynq_defconfig) and [`xilinx_defconfig`](https://github.com/Xilinx/linux-xlnx/blob/xilinx-v2026.1/arch/arm64/configs/xilinx_defconfig). Zynq gains explicit ext4, ACL and security-label support and removes redundant scheduler selections. ARM64 gains thermal support, CoreSight/default tracing, NVMe, Type-C, I3C, GPIO aggregation, RPMsg TTY and additional PCIe/PHY support; INA power monitors move from IIO to hwmon, and obsolete selections are removed. Koheron-specific settings are retained. Kconfig dependencies determine which drivers apply to each platform; these selections do not change the Vivado version or runtime kernel default.

Set `PASSWORD` and `TIMEZONE` in the environment before `make image` to override the defaults in [rootfs.mk](./rootfs.mk).

The image manifest records source tags separately from `vivado`, `vivado_build`,
`vitis` and `vitis_build`, queried from `VIVADO_PATH` and `VITIS_PATH` at packaging.
Unavailable values produce a warning and `unknown`. This identifies the selected
tools, not the history of cached artifacts; rebuild artifacts after changing tools.

The image uses glibc's built-in `C.UTF-8` locale. UTF-8 text remains supported,
with C sorting and formatting conventions. To use another locale, install
`locales`, generate the desired locale and run `update-locale`. Image finalization
removes APT caches and package documentation, including files already present in
Ubuntu Base, while retaining copyright notices and runtime encoding data.

uWSGI starts eagerly alongside other services and automatically inherits the
Unix socket from systemd. Its Python initialization does not gate `basic.target`.
The IP LED helper waits for an IPv4 address on `end0` or legacy `eth0` in the
background, including when DHCP arrives after boot. It does not pull in
`network-online.target`. A completed boot does not imply that DHCP or the web API
is already ready; measure those separately. The helper does not monitor later
address changes after displaying the IP.

## Runtime FPGA loading

Runtime instrument overlays describe devices only. The server removes the previous
overlay, programs the full bitstream through the Xilinx FPGA Manager `firmware`
attribute, verifies its `operating` state, then applies `pl.dtbo`. The generated
overlay omits `firmware-name` and exported `__symbols__`, while retaining external
and local phandle fixups. This avoids retaining properties on permanent
device-tree nodes each time an instrument is switched. Boot-time board overlays
still export symbols.

Rebuild the server and `pl.dtbo` together when updating an existing instrument;
the bitstream itself does not need rebuilding for this loading change. The direct
loading path requires Xilinx's FPGA Manager sysfs interface (available in the
2025.1 and 2026.1 kernels) and supports full-device designs without FPGA bridges.
Bridge-dependent custom designs are rejected before removing the current overlay.
Partial reconfiguration and stacked runtime overlays are not supported by this
path. The legacy `/dev/xdevcfg` path is unchanged.

The loader checks both the configfs overlay path and its status: Xilinx configfs
can report `applied` after rejecting a malformed DTBO, but clears its path on
failure. A failed load exits before driver initialization and lets the instrument
installer restore and reprogram the previous installation.

## Tests

Image-build tests are in [tests/](./tests/); instrument loading tests are in [api/tests/](./api/tests/).

## Runtime kernel features

The Zynq and ZynqMP defconfigs build in Unix socket diagnostics for the packaged
uWSGI backlog monitor, autofs, UTS/network namespaces, cgroup BPF and nftables for
systemd services, and SysRq/Yama for the distribution's sysctl settings. These
features are built in because the OS image does not install kernel modules.
The kernel log buffer is 128 KiB to retain boot diagnostics before journald starts.

After changing either defconfig, check the generated kernel `.config`, rebuild
the OS image, and verify boot logs, SSH, the management API and instrument
switching on the target board. A kernel rebuild does not require an FPGA rebuild.

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
