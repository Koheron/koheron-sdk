# STEMlab 125-14 PRO Z7020 Gen 2

Build target: `red-pitaya-pro-z7020-gen2`. See the
[board comparison and build guide](../README.md).

Build an image that boots FFT and includes PNA:

```sh
./build.sh red-pitaya-pro-z7020-gen2
```

The selectors reuse the shared FPGA designs, C++ drivers and web interfaces.
FFT uses nominal LV-input calibration. Linux reserves the last 512 MiB of the
1 GiB DDR exclusively for DMA; U-Boot keeps its allocations below `0x20000000`.
PNA selects `memory-1g.yml`, placing its 32 MiB DMA window at `0x3e000000`.
Its registers, processing parameters and buffer size match the other models.
The LED/register bring-up instrument remains available.
