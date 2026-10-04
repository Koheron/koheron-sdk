# OS image builds

Build and installation: [SDK quick start](../README.md#quick-start).

## Settings

Set `PASSWORD` and `TIMEZONE` in the environment before `make image` to override the defaults in [rootfs.mk](./rootfs.mk).

## Boot behavior

ALPHA250 keeps a one-second U-Boot countdown for serial console access.
At boot, FPGA Manager systems extract the runtime `.bit.bin` and skip the
reference `.bit`, which remains available in the instrument ZIP. Systems using
the legacy `/dev/xdevcfg` loader retain the `.bit` during extraction.

The HTTP API initializes through uWSGI during boot, so the first API request
does not trigger Python/Flask startup. nginx serves static files independently
of the API.

The default boot command line uses `loglevel=5`: kernel warnings and errors
remain visible on the serial console, while informational messages are kept
in the kernel log without being printed over the 115200-baud link. Serial
login and U-Boot access remain available. Board-specific boot templates, such
as the KR260 debug template, have their own console settings.

The shared Zynq-7000 configuration omits video capture, DRM display, audio
and PCI/PCIe drivers, along with Versatile Express and big.LITTLE platform
support. SD-card and USB storage, serial, Ethernet, CAN, SPI/I2C, GPIO, FPGA
Manager, overlays, UIO and DMA support remain enabled. Instruments needing a
removed feature must enable it in [xilinx_zynq_defconfig](./xilinx_zynq_defconfig)
before rebuilding. The Zynq UltraScale+ configuration is separate.

## Measuring startup

Run on the board:

```bash
systemd-analyze time
systemd-analyze critical-chain koheron-server.service
systemd-analyze critical-chain multi-user.target
systemd-analyze blame
journalctl -b -o short-monotonic -u koheron-server.service
```

Compare several boots after first-boot filesystem expansion and SSH key
generation. Use a timestamped serial capture to measure FSBL and U-Boot time
when systemd does not report it. The server reports readiness after FPGA loading,
hardware initialization and listener startup; network address assignment and
the first HTTP API response are separate milestones.

For kernel profiling, temporarily replace `loglevel=5` with
`loglevel=8 initcall_debug printk.time=1` in the boot partition's
`extlinux/extlinux.conf`, then capture `journalctl -b -k -o short-monotonic`
after reboot. Restore the normal command line after collecting the log;
verbose serial output adds overhead to the profiling boot. Compare normal
boots to establish the actual speedup.

Benchmark GZIP against LZ4 compression on the target as well: LZ4 trades a
larger kernel image for faster decompression, so SD-card transfer time affects
the result.

## Tests

Image-build tests are in [tests/](./tests/); instrument loading tests are in [api/tests/](./api/tests/).
