# koheron-sdk

[![Circle CI](https://circleci.com/gh/Koheron/koheron-sdk.svg?style=shield)](https://circleci.com/gh/Koheron/koheron-sdk)

#### `Software Development Kit for Zynq-based instruments`

#### Supported Base Boards

The [Red Pitaya](http://redpitaya.com) is used as the reference board. Please contact us at `hello@koheron.com` if you need another board to be supported.

#### Examples of instruments

* [`blink`](https://github.com/Koheron/koheron-sdk/tree/master/projects/blink) : minimal instrument with access to LEDs and Red Pitaya ADCs and DACs.
* [`oscillo`](https://github.com/Koheron/koheron-sdk/tree/master/projects/oscillo) : signal acquisition / generation with coherent averaging mode.
* [`spectrum`](https://github.com/Koheron/koheron-sdk/tree/master/projects/spectrum) : spectrum analyzer with peak-detection and averaging.

## Quick start

You can find the latest release of the SD card image for the Red Pitaya `oscillo-<version>.img` on this [link](https://github.com/Koheron/koheron-sdk/releases). The [`oscillo`](https://github.com/Koheron/koheron-sdk/tree/master/projects/oscillo) and [`spectrum`](https://github.com/Koheron/koheron-sdk/tree/master/projects/spectrum) instruments are preinstalled.

1. [Connect the board](http://www.koheron.com/products/laser-development-kit/getting-started/) to your computer.
2. Navigate to your board ip address (e.g. `192.168.1.18`) with your browser.

## Build your own instrument

The build is tested on Ubuntu 16.04.
[Install Vivado 2016.2](https://github.com/Koheron/koheron-sdk/issues/101) and source it (`source scripts/settings.sh`):

An instrument consists of a file `<instrument>-<version>.zip` that contains the bitstream and its corresponding server.
Run the instrument on your Zynq board and test it:
```
$ make NAME=<instrument> HOST=192.168.1.100 run test
```

## Build SD card image

Build zip, boot-loader and Linux kernel, then Ubuntu root file system:
```
$ make NAME=<instrument>
$ sudo bash scripts/image.sh <instrument>
```
