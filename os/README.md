# OS images

## Building and flashing

Build a board image from the SDK root:

```sh
./build.sh alpha250
```

The board preset in `boards/alpha250/build_image.conf` selects FFT as the
default instrument and adds the other ALPHA250 instruments. Run
`./build.sh --list` for other boards.

### Direct image build

To build an image containing only one instrument:

```sh
make -j CFG=examples/alpha250/fft/config.mk image
```

The release ZIP, `tmp/examples/alpha250/fft/alpha250-fft.zip`, contains the
bootable `.img`. Use [instrument deployment](../docs/development.md#deployment-and-logs)
for subsequent instrument changes; rebuild the image when its OS, boot files,
board support or default instruments change.

### Flashing an existing image

```sh
sudo --preserve-env=PATH make CFG=examples/alpha250/fft/config.mk FLASH_DEVICE=/dev/sdX flash
```

Replace `/dev/sdX` with the SD card's whole-disk device. Flashing erases the
card. `flash` writes the existing release ZIP.

## Settings

The default image uses Ubuntu Base 26.04.1, Xilinx Linux/U-Boot 2026.1, and
Vivado/Vitis 2025.1. Select other releases with `LINUX_VERSION`,
`UBOOT_VERSION`, or `VIVADO_VERSION`. Set `PASSWORD` and `TIMEZONE` in the
environment to override the image defaults.

FTDI USB serial ports use a 1 ms receive latency timer by default, applied at
boot and on connection by `/etc/udev/rules.d/99-koheron-ftdi.rules` when the
device supports it. Rebuild the OS image to include this default.

Boards can set `FSBL_STDIO` and `PMU_STDIO` to a native UART peripheral name
to select each standalone BSP's standard input/output. Empty values retain
the vendor defaults. `PMU_CFLAGS` adds application compiler flags, for example
to select a vendor board configuration. PMU generation and native Vitis
compilation are separate steps; changing its console or flags regenerates
the application and discards cached objects.

`ATF_CONSOLE` selects the native ZynqMP Trusted Firmware console, defaulting
to `cadence` (UART0). Select `cadence1` for UART1. Changing it invalidates
cached Trusted Firmware objects.

The [board management guide](api/README.md) covers the API and web interface.
