# Fixed physical DMA memory

ALPHA250 instruments, including ALPHA250-4, use the physical range
`0x18000000`–`0x1fffffff` directly through `mem_wc`. A reusable CMA pool does
not grant ownership of those pages: Linux can allocate them until a driver
claims them through the DMA allocation API. Using the range directly can
overwrite process memory, filesystem caches and the analyzer's results.

The board boot overlay reserves this 128 MiB range with `no-map`, without
`reusable` or `linux,cma`. It leaves 384 MiB for Linux; the kernel's separate
default CMA allocation remains available. Applications using `/dev/cma` must
respect that separate pool's available capacity rather than assuming the
fixed instrument ring is allocatable.

The server rejects fixed `mem_wc` windows overlapping `/proc/iomem` System
RAM before mapping them, including its `/dev/mem` fallback. Missing or
redacted RAM information also fails closed. FPGA register windows above RAM
remain usable. This protects an updated instrument loaded on an older SD image.

Updating an instrument ZIP alone cannot reserve memory already in use by
Linux. Rebuild the SD boot image with the corrected board overlay and reboot.
Before starting acquisition, check that `/proc/iomem` ends System RAM at
`17ffffff` and that the live device tree contains
`/reserved-memory/dma-buffer@18000000/no-map`. Retain a boot image backup
when updating an existing SD card.

Run the boundary, overlap, malformed/redacted input and 64-bit address checks:

```sh
g++ -std=c++20 -Wall -Wextra -fsanitize=address,undefined -I. \
    os/tests/dma-reservation/test_system_ram.cpp -o /tmp/test-system-ram
/tmp/test-system-ram
python3 os/tests/dma-reservation/test_overlay.py
```

Hardware validation on ALPHA250-4: before the boot change, a 500,000-record
I/Q capture contained 25,905 overwritten timestamps and phase-DMA words in
its heap buffer. After booting the exclusive reservation, another 500,000-record
capture had no invalid timestamps or non-increasing intervals. Source native
settings remained unchanged at 10 MHz with 1° sine PM at 10 kHz during the
post-fix capture. Noise-floor calibration remains a separate measurement.
