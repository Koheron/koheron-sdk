# zynq-sdk

[![Circle CI](https://circleci.com/gh/Koheron/zynq-sdk.svg?style=shield)](https://circleci.com/gh/Koheron/zynq-sdk)
[![Code Climate](https://codeclimate.com/github/Koheron/zynq-sdk/badges/gpa.svg)](https://codeclimate.com/github/Koheron/zynq-sdk)

#### `Software Development Kit for Zynq-based instruments`

* Ubuntu Core 16.04 LTS
* Vivado 2016.1 toolchain (Linux Kernel version 4.4)
* User-space control of hardware with [TCP / Websocket server](https://github.com/Koheron/tcp-server)
* HTTP, Python and Javascript APIs

#### Supported Base Boards

The [Red Pitaya](http://redpitaya.com) is used as a reference board for this project. Please contact us at `hello@koheron.com` if you need another board to be supported.

#### Available instruments

* [`oscillo`](https://github.com/Koheron/zynq-sdk/tree/master/projects/oscillo) : simple signal acquisition / generation with coherent averaging mode.
* [`spectrum`](https://github.com/Koheron/zynq-sdk/tree/master/projects/spectrum) : spectrum analyzer with peak-detection and averaging.

## Quick start

You can find the latest release of the SD card image for the Red Pitaya `oscillo-<version>.img` on this [link](https://github.com/Koheron/zynq-sdk/releases). The [`oscillo`](https://github.com/Koheron/zynq-sdk/tree/master/projects/oscillo) and [`spectrum`](https://github.com/Koheron/zynq-sdk/tree/master/projects/spectrum) instruments are preinstalled.

1. [Connect the board](http://www.koheron.com/products/laser-development-kit/getting-started/) to your computer.
2. Navigate to your board ip address (e.g. `192.168.1.100`) with your browser.

![Web interface](https://cloud.githubusercontent.com/assets/1735094/16599901/d9a205ea-4304-11e6-9303-4f02c1aedb4d.png)

## Build your own instrument

The build is tested on Ubuntu 16.04.
[Install Vivado 2016.1](https://github.com/Koheron/zynq-sdk/issues/101) and source it (`source scripts/settings.sh`):

An instrument consist of a file `<instrument>-<version>.zip` that contains the bitstream and its corresponding server.
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
