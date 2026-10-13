# STEMlab 125-14 Gen 2

Build target: `red-pitaya-gen2`. See the
[board comparison and build guide](../README.md).

This is the standard Gen 2 Z7010/512 MiB package. It uses the shared Red Pitaya
ADC/DAC cores and drivers, with the Gen 2 SD-selection preset and an exclusive
128 MiB DMA reservation. Its image preset boots FFT and includes PNA:

```sh
./build.sh red-pitaya-gen2
```

Both instruments share their FPGA, drivers and web interfaces with the other
STEMlab models. FFT uses nominal LV-input calibration. The LED/register
bring-up instrument remains available. External ADC clock selection, S1/S2
synchronization and E3 boot expansion are available on the PRO models.
