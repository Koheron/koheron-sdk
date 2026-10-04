# OS image builds

Build and installation: [SDK quick start](../README.md#quick-start).

## Settings

Set `PASSWORD` and `TIMEZONE` in the environment before `make image` to override the defaults in [rootfs.mk](./rootfs.mk).

## Boot behavior

- FPGA Manager boot extraction skips the reference `.bit`; legacy
  `/dev/xdevcfg` systems retain it. Both formats remain in instrument ZIPs.
- The default `loglevel=5` prints kernel warnings and errors to serial;
  informational messages remain available through `dmesg`.
- The [Zynq-7000 configuration](./xilinx_zynq_defconfig) omits video, display,
  audio, PCI/PCIe, Versatile Express and big.LITTLE support. CAN remains enabled.
  Enable omitted features there before rebuilding if your instrument needs them.

## Tests

Image-build tests are in [tests/](./tests/); instrument loading tests are in [api/tests/](./api/tests/).
