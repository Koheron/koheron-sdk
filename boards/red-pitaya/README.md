# Red Pitaya STEMlab boards

The SDK groups the original STEMlab 125-14 and the three Gen 2 variants here.
Each model has a `stemlab-*` board package. The Gen 2 packages reuse the original
board's ADC/DAC cores, drivers, U-Boot patches and common converter constraints.

## Hardware differences

| Board | FPGA | DDR | Clock and synchronization | E3 expansion |
| --- | --- | --- | --- | --- |
| STEMlab 125-14 (original) | Zynq-7010 | 512 MiB, 16-bit | Internal clock; SATA daisy-chain links | None |
| STEMlab 125-14 Gen 2 | Zynq-7010 | 512 MiB, 16-bit | Internal clock; no S1/S2 links | None |
| STEMlab 125-14 PRO Gen 2 | Zynq-7010 | 512 MiB, 16-bit | Internal/external ADC clock; USB-C S1/S2 links | Optional QSPI/eMMC boot module |
| STEMlab 125-14 PRO Z7020 Gen 2 | Zynq-7020 | 1 GiB, 32-bit | Internal/external ADC clock; USB-C S1/S2 links | Optional boot module and eight high-speed differential pairs |

All four have two 14-bit ADC inputs and two 14-bit DAC outputs at 125 MS/s.
The Gen 2 family improves the analog front end and uses USB-C for power and
console. Standard Gen 2 and PRO Gen 2 have the same FPGA, RAM and specified
converter performance. PRO adds clock selection, board synchronization and
boot expansion; PRO Z7020 also increases FPGA resources and RAM.
Some original-board variants support external clocks; the original package
targets the standard board. See the
[official comparison](https://redpitaya.readthedocs.io/en/latest/developerGuide/hardware/GEN2/compares/vs.html)
for the full hardware specifications.

S1/S2 use USB-C connectors for dedicated inter-board links. They are separate
from the USB host and console ports. PRO external ADC clock selection is a
physical E2 `CLK_SEL` input. The SDK presets use the internal 125 MHz clock and
SD boot; optional E3 boot and synchronization features require an instrument
configuration that uses them. TI converter variants need separate support.

## Packages and build targets

| Directory under `boards/red-pitaya/` | Build target |
| --- | --- |
| [stemlab-125-14](stemlab-125-14/README.md) | `red-pitaya` |
| [stemlab-125-14-gen2](stemlab-125-14-gen2/README.md) | `red-pitaya-gen2` |
| [stemlab-125-14-pro-gen2](stemlab-125-14-pro-gen2/README.md) | `red-pitaya-pro-gen2` |
| [stemlab-125-14-pro-z7020-gen2](stemlab-125-14-pro-z7020-gen2/README.md) | `red-pitaya-pro-z7020-gen2` |

`BOARD_PATH` selects the package, for example:

```make
BOARD_PATH := $(SDK_PATH)/boards/red-pitaya/stemlab-125-14-pro-gen2
```

The existing build targets and device-tree compatibility identifiers are
retained. `build.sh` discovers the nested `build_image.conf`
files through their `BOARD_TARGET` values. Use `./build.sh --list` to list them.

## Build an image

All four image presets build FFT as the default instrument and include PNA in
the instrument manager. Select the board's build target:

```sh
./build.sh red-pitaya
./build.sh red-pitaya-gen2
./build.sh red-pitaya-pro-gen2
./build.sh red-pitaya-pro-z7020-gen2
```

Examples are grouped by instrument under `examples/red-pitaya/`. Board selectors
sit beside the shared implementation:

```text
examples/red-pitaya/fft/
  common.mk
  stemlab-125-14.mk
  stemlab-125-14-gen2.mk
  stemlab-125-14-pro-gen2.mk
  stemlab-125-14-pro-z7020-gen2.mk
  block_design.tcl
  memory.yml
  drivers/
  web/
```

Choose a specific selector when building an instrument:

```sh
make CFG=examples/red-pitaya/fft/stemlab-125-14-pro-gen2.mk
make CFG=examples/red-pitaya/phase-noise-analyzer/stemlab-125-14-pro-gen2.mk
```

Selectors include the instrument's `common.mk`, selecting the board preset
while sharing FPGA designs, C++ drivers and web interfaces. The 512 MiB models
share PNA's memory map; the 1 GiB model selects `memory-1g.yml` to place its DMA
buffer in that board's reserved DDR tail. Gen 2 FFT selectors use nominal LV
input calibration. See the PRO Gen 2
[package guide](stemlab-125-14-pro-gen2/README.md) for calibration details.
The shared LED/register instrument remains available for bring-up on all four
boards.

To build that instrument for a specific board:

```sh
make fpga CFG=examples/red-pitaya/led-blinker/stemlab-125-14-gen2.mk
make image CFG=examples/red-pitaya/led-blinker/stemlab-125-14-gen2.mk
```

For either PRO model, select its `stemlab-*.mk` in the same LED instrument
directory. Generated artifacts are separated by selector under
`tmp/examples/red-pitaya/<instrument>/<selector>/`. `make list`, `make check`
and CI discover named selectors as well as the other boards' `config.mk` files.
For Vivado/Vitis
2026.1, set `VIVADO_VERSION=2026.1` on make commands or use
`MAKEFLAGS='VIVADO_VERSION=2026.1' ./build.sh ...`.

## Memory and boot

FSBL initializes the full DDR capacity and the selected bus width. Linux
describes the full DDR and excludes its tail with an exclusive `no-map`
reservation. U-Boot keeps boot allocations below that reservation:

| Board capacity | DMA reservation | U-Boot allocation limit |
| --- | --- | --- |
| 512 MiB | Last 128 MiB, `0x18000000..0x1fffffff` | First 384 MiB |
| 1 GiB | Last 512 MiB, `0x20000000..0x3fffffff` | First 512 MiB |

Fixed instrument DMA buffers must lie inside the reserved region. PNA's 32 MiB
buffer starts at `0x1e000000` on 512 MiB boards and `0x3e000000` on the 1 GiB
board, within the respective reservation.
Allowing U-Boot to place boot data in the DMA tail can make Linux reject the
reservation and prevent PNA from starting.

Gen 2 presets disable SD card-detect/write-protect functions on MIO46/47:
MIO46 selects SD/eMMC and is left at its SD pull-up. The packages reuse the
read-only EEPROM identity reader for `ethaddr`, `hw_rev` and `serial` at
`0x1804`. Factory identity and calibration data are not rewritten.

## Validation

Run the Gen 2 PS/GPIO checks and compiled device-tree checks with:

```sh
vivado -mode batch -source boards/red-pitaya/stemlab-125-14-gen2/tests/check_bsp.tcl
python3 boards/red-pitaya/stemlab-125-14-gen2/tests/test_device_tree.py
```

Build validation with Vivado/Vitis 2026.1 covered all three Gen 2 LED/register
bitstreams, FSBLs and boot images, plus ADC/DAC builds on Z7010 and Z7020.
PRO Gen 2 FFT and PNA passed routed setup/hold timing and image verification.

Physical tests on the PRO Gen 2 Z7010/512 MiB board passed boot with the DMA
reservation, FFT loopback at 7.8125 and 15.625 MHz, and PNA loopback with 0.1 rad
sine PM at 6103.515625 Hz (0.11% integrated phase-power error). These checks used
OUT1 to IN1 with the LV input range. Other variants still require physical
testing; absolute amplitude calibration is unverified.

## Hardware references

Gen 2 pin and memory configurations use the V2r0 RevA development schematics
and official FPGA sources:

- [Standard Gen 2 schematics](https://downloads.redpitaya.com/doc/Schematics/Schematics_STEM_125-14_Gen2_V2r0_RevA.pdf)
- [PRO Gen 2 schematics](https://downloads.redpitaya.com/doc/Schematics/Schematics_STEM_125-14_PRO_Gen2_V2r0_RevA.pdf)
- [PRO Z7020 Gen 2 schematics](https://downloads.redpitaya.com/doc/Schematics/Schematics_STEM_125-14_PRO_Z7020_Gen2_V2r0_RevA.pdf)
- [Gen 2 FPGA constraints](https://github.com/RedPitaya/RedPitaya-FPGA/blob/728a4f37e9c0a9b5ba1d9b7a0d44c38bbd2dc674/prj/v0.94/sdc/red_pitaya_G2.xdc)
