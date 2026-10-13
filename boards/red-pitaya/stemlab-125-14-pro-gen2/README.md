# STEMlab 125-14 PRO Gen 2

Build target: `red-pitaya-pro-gen2`. See the
[board comparison and build guide](../README.md).

## FFT and phase-noise analyzer image

Build an SD image with FFT as the default instrument and PNA available from
the instrument manager:

```sh
MAKEFLAGS='VIVADO_VERSION=2026.1' N_CPUS=16 ./build.sh red-pitaya-pro-gen2
```

The two instrument configs include the existing Red Pitaya configs. They reuse
the FPGA designs, memory maps, C++ drivers and web interfaces, while selecting
the PRO Gen 2 Z7010 preset, pins and boot support. The LED/register instrument
remains available for bring-up.

The image uses the on-board 125 MHz clock and SD boot. The last 128 MiB of its
512 MiB DDR is reserved exclusively for DMA; Linux has 384 MiB before other
boot reservations. PNA's fixed 32 MiB buffer lies within that reserved region.
FFT uses nominal +/-1 V LV-input scaling, zero offset and unity frequency
response rather than the older board's measured coefficients. PNA also uses
nominal LV-input scaling.
Physical boot, the exclusive DMA reservation, FFT loopback and PNA sine-PM
loopback passed on the Z7010/512 MiB board. The PNA hardware test starts a fresh
capture after each stimulus change so queued DMA data belongs to that stimulus.
Absolute amplitude calibration remains unverified. Factory EEPROM data is not
rewritten.
