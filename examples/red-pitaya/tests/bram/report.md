## Using /dev/mem

[Oct 07 10:53:31] FpgaManager: Bitstream loading method: fpga_manager
[Oct 07 10:53:31] FpgaManager: Loading bitstream bram.bit.bin...
[Oct 07 10:53:31] FpgaManager: Bitstream successfully loaded
[Oct 07 10:53:31] Memory [MemId1] Opening /dev/mem
[Oct 07 10:53:31] Memory [MemId0] Opening /dev/mem
[Oct 07 10:53:31] ZynqFclk: Found /sys/devices/soc0/fpga-region/fpga-region:clocking0
[Oct 07 10:53:31] ZynqFclk: amba:clocking0 set to 187500000 Hz
[Oct 07 10:53:31] ZynqFclk: amba:clocking0 rate is 187499999 Hz
[Oct 07 10:53:35] AXI4LITE WRITE= 4.347008425 s (15.1 MB/s)
[Oct 07 10:53:37] AXI4LITE WRITE (MEMCPY) = 2.332211105 s (28.1 MB/s)
[Oct 07 10:53:41] AXI4 WRITE = 4.000513041 s (16.4 MB/s)
[Oct 07 10:53:44] AXI4 WRITE (MEMCPY) = 2.062823329 s (31.8 MB/s)
[Oct 07 10:53:46] AXI4LITE READ = 2.172677815 s (30.2 MB/s) [sum=5003771109376000]
[Oct 07 10:53:48] AXI4 READ = 2.224141285 s (29.5 MB/s) [sum=5003771109376000]

## Using /dev/bram_wc

[Oct 07 10:52:09] FpgaManager: Bitstream loading method: fpga_manager
[Oct 07 10:52:09] FpgaManager: Loading bitstream bram.bit.bin...
[Oct 07 10:52:09] FpgaManager: Bitstream successfully loaded
[Oct 07 10:52:09] Memory [MemId1] Opening /dev/mem_wc0x42000000
[Oct 07 10:52:09] Memory [MemId0] Opening /dev/mem_wc0x40000000
[Oct 07 10:52:09] ZynqFclk: Found /sys/devices/soc0/fpga-region/fpga-region:clocking0
[Oct 07 10:52:09] ZynqFclk: amba:clocking0 set to 187500000 Hz
[Oct 07 10:52:09] ZynqFclk: amba:clocking0 rate is 187499999 Hz
[Oct 07 10:52:09] AXI4LITE WRITE= 0.41887156 s (156.5 MB/s)
[Oct 07 10:52:10] AXI4LITE WRITE (MEMCPY) = 0.335267222 s (195.5 MB/s)
[Oct 07 10:52:10] AXI4 WRITE = 0.138700266 s (472.5 MB/s)
[Oct 07 10:52:10] AXI4 WRITE (MEMCPY) = 0.113007344 s (579.9 MB/s)
[Oct 07 10:52:10] AXI4LITE READ = 0.658957299 s (99.5 MB/s) [sum=5003771109376000]
[Oct 07 10:52:11] AXI4 READ = 0.678622936 s (96.6 MB/s) [sum=5003771109376000]