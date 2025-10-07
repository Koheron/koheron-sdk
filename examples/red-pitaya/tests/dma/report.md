# Using /dev/mem

```
[Oct 07 18:17:21] FpgaManager: Bitstream loading method: fpga_manager
[Oct 07 18:17:21] FpgaManager: Loading bitstream dma.bit.bin...
[Oct 07 18:17:21] FpgaManager: Bitstream successfully loaded
[Oct 07 18:17:21] Memory [MemId0] Opening /dev/mem
[Oct 07 18:17:21] Memory [MemId1] Opening /dev/mem
[Oct 07 18:17:21] Memory [MemId2] Opening /dev/mem
[Oct 07 18:17:21] Memory [MemId3] Opening /dev/mem
[Oct 07 18:17:21] Memory [MemId4] Opening /dev/mem
[Oct 07 18:17:21] Memory [MemId5] Opening /dev/mem
[Oct 07 18:17:21] ZynqFclk: Found /sys/devices/soc0/fpga-region/fpga-region:clocking0
[Oct 07 18:17:21] ZynqFclk: amba:clocking0 set to 100000000 Hz
[Oct 07 18:17:21] ZynqFclk: amba:clocking0 rate is 99999999 Hz
[Oct 07 18:17:22] Prep TX (write_array): 71.24 MiB/s | Clear RX: 80.29 MiB/s
[Oct 07 18:17:23] DMA moved 33554432 bytes in 42.149 ms -> 759.21 MiB/s | checksum=35184367894528/35184367894528
```

# Using /dev/mem_wc

```
memory:
  - name: ram_mm2s
    offset: '0x18000000'
    range: 64M
    compatible: "koheron,mem-wc-1.0"
    dev: '/dev/mem_wc'
  - name: ram_s2mm
    offset: '0x1C000000'
    range: 64M
    compatible: "koheron,mem-wc-1.0"
```

```
[Oct 07 18:18:39] FpgaManager: Bitstream loading method: fpga_manager
[Oct 07 18:18:39] FpgaManager: Loading bitstream dma.bit.bin...
[Oct 07 18:18:39] FpgaManager: Bitstream successfully loaded
[Oct 07 18:18:39] Memory [MemId0] Opening /dev/mem_wc0x18000000
[Oct 07 18:18:39] Memory [MemId1] Opening /dev/mem_wc0x1C000000
[Oct 07 18:18:39] Memory [MemId2] Opening /dev/mem
[Oct 07 18:18:39] Memory [MemId3] Opening /dev/mem
[Oct 07 18:18:39] Memory [MemId4] Opening /dev/mem
[Oct 07 18:18:39] Memory [MemId5] Opening /dev/mem
[Oct 07 18:18:39] ZynqFclk: Found /sys/devices/soc0/fpga-region/fpga-region:clocking0
[Oct 07 18:18:39] ZynqFclk: amba:clocking0 set to 100000000 Hz
[Oct 07 18:18:39] ZynqFclk: amba:clocking0 rate is 99999999 Hz
[Oct 07 18:18:39] Prep TX (write_array): 217.91 MiB/s | Clear RX: 299.38 MiB/s
[Oct 07 18:18:40] DMA moved 33554432 bytes in 43.239 ms -> 740.07 MiB/s | checksum=35184367894528/35184367894528
```