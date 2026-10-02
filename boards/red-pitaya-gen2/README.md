# Red Pitaya STEMlab 125-14 Gen2 board support

| `BOARD_PATH` | FPGA | DDR |
| --- | --- | --- |
| `boards/red-pitaya-gen2` | XC7Z010-1CLG400 | 512 MiB, 16-bit bus |
| `boards/red-pitaya-pro-gen2` | XC7Z010-1CLG400 | 512 MiB, 16-bit bus |
| `boards/red-pitaya-pro-z7020-gen2` | XC7Z020-1CLG400 | 1 GiB, 32-bit bus |

These packages target the non-TI Gen2 V2r0 boards, SD boot and the on-board
125 MHz ADC clock. Each supplies a PS preset, ADC/DAC and connector pin
constraints, Linux board overlay, U-Boot configuration and an image build
configuration. The ADC/DAC cores and board drivers are shared with the original
Red Pitaya.

## Build and use

Start with the LED/register bring-up instrument:

```sh
make fpga CFG=examples/red-pitaya-gen2/led-blinker/config.mk
make image CFG=examples/red-pitaya-gen2/led-blinker/config.mk
```

For the PRO or PRO Z7020, replace `red-pitaya-gen2` in `CFG` with the matching
board name from the table. `./build.sh --list` lists all three packages;
`./build.sh red-pitaya-pro-z7020-gen2` builds that board's SD image.

An instrument selects the board in its `config.mk`:

```make
BOARD_PATH := $(SDK_PATH)/boards/red-pitaya-pro-z7020-gen2
XDC += $(BOARD_PATH)/config/ports.xdc
XDC += $(BOARD_PATH)/config/clocks.xdc
include $(BOARD_PATH)/cores/cores.mk
```

Existing Red Pitaya block designs can use `$board_path/config/ports.tcl` and
`$board_path/base_system.tcl`. Keep the selected board's preset and XDC paths;
changing only the FPGA part is insufficient for the Z7020's larger DDR memory.
The provided LED instruments reuse the existing SDK LED/register design.

## Connectors

- E1 GPIO: source `$board_path/gpio.tcl`, call `add_gpio`, and add
  `$(BOARD_PATH)/config/expansion_connector.xdc` to `XDC`. The Z7010 models support
  up to 8 GPIOs per side; Z7020 supports 11. GPIOs 0..7 are 3.3 V. On Z7020,
  GPIOs 8..10 use Bank 13 at its factory 2.5 V configuration.
- PRO synchronization: `config/sync.xdc` supplies the existing `daisy_p_o`,
  `daisy_n_o`, `daisy_p_i`, `daisy_n_i` pin names for S1/S2. These connectors are
  dedicated inter-board links. Standard Gen2 has no S1/S2 connectors.
- Z7020 E3: source `e3.tcl` and call `add_e3_ports` when needed. Add
  `config/e3.xdc` for the four outgoing and four incoming LVDS pairs, and S1
  orientation/link inputs. The instrument supplies differential buffers,
  logic and timing constraints. These ports are optional, so a basic instrument
  does not expose unused E3 connections.
- External ADC clock selection is a physical E2 `CLK_SEL` input on the PRO
  boards. The converter interface defaults to the on-board 125 MHz clock.

The presets disable SD card-detect/write-protect functions on MIO46/47:
MIO46 is the Gen2 SD/eMMC selector, left at its SD pull-up. Optional E3 QSPI,
eMMC and watchdog operation require an application-specific configuration.

## Boot and identity

FSBL initialization is generated from the selected board's XSA, including its
DDR bus width. U-Boot and the Linux overlay expose the same total memory size;
Linux retains the SDK's 128 MiB CMA pool at `0x18000000`. The packages use the
existing read-only EEPROM identity reader (`ethaddr`, `hw_rev`, `serial` at
`0x1804`). They do not rewrite factory calibration or board identity.

The existing FFT calibration coefficients belong to the older board. A Gen2
measurement instrument must supply calibration appropriate to its hardware;
calibration is outside these board support packages.

## Validation

Run the Vivado preset and connector checks with:

```sh
vivado -mode batch -source boards/red-pitaya-gen2/tests/check_bsp.tcl
```

Validated with Vivado/Vitis 2026.1 and U-Boot xilinx-v2026.1:

- All three LED/register instruments: bitstream, FSBL, U-Boot and `boot.bin`.
- Routed timing enforcement passed for all three instruments.
- The existing ADC/DAC example also passed routed timing with the Gen2 Z7010
  and Z7020 packages (setup slack 0.506 ns and 0.504 ns respectively).
- Real Vivado PS/GPIO checks passed for all three presets.
- Linux overlays compiled and matched the U-Boot and FSBL memory sizes.

Physical-board boot and analog I/O operation have not been tested.

## Hardware references

Pin and memory configurations are based on the published V2r0 RevA development
schematics and the official FPGA sources at commit
`728a4f37e9c0a9b5ba1d9b7a0d44c38bbd2dc674`:

- [Standard Gen2 schematics](https://downloads.redpitaya.com/doc/Schematics/Schematics_STEM_125-14_Gen2_V2r0_RevA.pdf)
- [PRO Z7020 Gen2 schematics](https://downloads.redpitaya.com/doc/Schematics/Schematics_STEM_125-14_PRO_Z7020_Gen2_V2r0_RevA.pdf)
- [Gen2 FPGA constraints](https://github.com/RedPitaya/RedPitaya-FPGA/blob/728a4f37e9c0a9b5ba1d9b7a0d44c38bbd2dc674/prj/v0.94/sdc/red_pitaya_G2.xdc)

The TI converter variants and other Z7020 board families need separate packages.
