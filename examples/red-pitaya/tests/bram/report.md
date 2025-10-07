## Using /dev/mem

[Oct 07 12:05:09] FpgaManager: Bitstream loading method: fpga_manager
[Oct 07 12:05:09] FpgaManager: Loading bitstream bram.bit.bin...
[Oct 07 12:05:09] FpgaManager: Bitstream successfully loaded
[Oct 07 12:05:09] Memory [MemId1] Opening /dev/mem
[Oct 07 12:05:09] Memory [MemId0] Opening /dev/mem
[Oct 07 12:05:09] ZynqFclk: Found /sys/devices/soc0/fpga-region/fpga-region:clocking0
[Oct 07 12:05:09] ZynqFclk: amba:clocking0 set to 187500000 Hz
[Oct 07 12:05:09] ZynqFclk: amba:clocking0 rate is 187499999 Hz
[Oct 07 12:05:13] AXI4LITE WRITE= 4.393306565 s (14.9 MB/s)
[Oct 07 12:05:16] AXI4LITE WRITE (MEMCPY) = 2.331754363 s (28.1 MB/s)
[Oct 07 12:05:20] AXI4 WRITE = 4.002314017 s (16.4 MB/s)
[Oct 07 12:05:22] AXI4 WRITE (MEMCPY) = 2.063218795 s (31.8 MB/s)
[Oct 07 12:05:24] AXI4LITE READ = 2.120557623 s (30.9 MB/s)
[Oct 07 12:05:26] AXI4 READ = 2.172479828 s (30.2 MB/s)

## Using /dev/bram_wc

[Oct 07 12:06:13] FpgaManager: Bitstream loading method: fpga_manager
[Oct 07 12:06:13] FpgaManager: Loading bitstream bram.bit.bin...
[Oct 07 12:06:13] FpgaManager: Bitstream successfully loaded
[Oct 07 12:06:13] Memory [MemId1] Opening /dev/mem_wc0x42000000
[Oct 07 12:06:13] Memory [MemId0] Opening /dev/mem_wc0x40000000
[Oct 07 12:06:13] ZynqFclk: Found /sys/devices/soc0/fpga-region/fpga-region:clocking0
[Oct 07 12:06:13] ZynqFclk: amba:clocking0 set to 187500000 Hz
[Oct 07 12:06:13] ZynqFclk: amba:clocking0 rate is 187499999 Hz
[Oct 07 12:06:13] AXI4LITE WRITE= 0.435246758 s (150.6 MB/s)
[Oct 07 12:06:14] AXI4LITE WRITE (MEMCPY) = 0.353166668 s (185.6 MB/s)
[Oct 07 12:06:14] AXI4 WRITE = 0.141224126 s (464.1 MB/s)
[Oct 07 12:06:14] AXI4 WRITE (MEMCPY) = 0.110433039 s (593.4 MB/s)
[Oct 07 12:06:14] AXI4LITE READ = 0.605608149 s (108.2 MB/s)
[Oct 07 12:06:15] AXI4 READ = 0.627231927 s (104.5 MB/s)