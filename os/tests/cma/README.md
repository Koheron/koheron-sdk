# CMA ownership regression test

`test_cma.c` exercises the real `/dev/cma` driver. It does not program the FPGA.
Use an idle test system with a CMA pool of at least 32 MiB, or a disposable VM
with the patched driver built in. Do not define `NDEBUG`: assertions also execute
the operations under test.

```sh
cc -O2 -Wall -Wextra -Werror -pthread os/tests/cma/test_cma.c -o /tmp/test-cma
sudo /tmp/test-cma
```

The standard suite expects a CMA pool smaller than 4 GiB. It checks independent
opens, shared ownership through `dup`, mappings surviving descriptor closure,
VMA splitting, forked mappings, invalid ioctl and
mmap arguments, page offsets, zeroed allocations, failed replacement preserving
the previous buffer, allocation exhaustion, concurrent allocation/mapping,
repeated reclamation, and process exit without explicit cleanup.
Success ends with `ALL CMA TESTS PASSED`. It needs `/dev/cma`, fork and pthread
support; the reclamation test cumulatively allocates 64 MiB in a 32 MiB pool.

For VM validation, build an x86 kernel with the patched `cma.c` linked into
`drivers/misc/`, `CONFIG_CMA=y`, `CONFIG_DMA_CMA=y` and
`CONFIG_CMA_SIZE_MBYTES=32`. An initramfs can contain a statically linked test
binary, BusyBox, and an `/init` script that mounts devtmpfs and procfs, runs the
suite, prints its exit code, and powers off. Enable `CONFIG_BLK_DEV_INITRD`,
`CONFIG_RD_GZIP`, `CONFIG_BINFMT_ELF`, `CONFIG_BINFMT_SCRIPT`, `CONFIG_DEVTMPFS`,
`CONFIG_PROC_FS`, `CONFIG_SERIAL_8250_CONSOLE`, and `CONFIG_FUTEX`.

```sh
qemu-system-x86_64 -machine accel=tcg -cpu max -m 512 -smp 2 \
  -nographic -no-reboot -kernel path/to/bzImage \
  -initrd path/to/initramfs.cpio.gz \
  -append 'console=ttyS0 rdinit=/init cma=32M panic=-1'
```

A VM validates allocation and CPU mapping lifetime. Board validation must also
check the FPGA address, transfer shutdown, and cache behavior.

To verify address truncation is rejected, boot a separate 5 GiB VM with
`cma=32M@0x110000000` and run `test-cma --high-address`. This attempts 100
allocations from a pool above 4 GiB, expecting `EOVERFLOW` every time.

Validated locally with Xilinx 2026.1 (Linux 6.18) in QEMU, including the high
address case, with `CONFIG_DEBUG_VM=y`. The driver object also compiles for ARM
and ARM64 against Xilinx 2025.1 (Linux 6.12), using the SDK defconfigs, and
against Xilinx 2026.1. FPGA transfer and cache validation remains pending.
