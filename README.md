# zynq-sdk

[![Circle CI](https://circleci.com/gh/Koheron/zynq-sdk.svg?style=shield)](https://circleci.com/gh/Koheron/zynq-sdk)
[![Code Climate](https://codeclimate.com/github/Koheron/zynq-sdk/badges/gpa.svg)](https://codeclimate.com/github/Koheron/zynq-sdk)

#### `Software Development Kit for Zynq-based instruments`

* Ubuntu Core 16.04 LTS
* Vivado 2016.1 toolchain (Linux Kernel version 4.4)
* User-space control of hardware with [TCP / Websocket server](https://github.com/Koheron/tcp-server)
* HTTP, Python and Javascript APIs

### Supported Base Boards

* [Red Pitaya](http://redpitaya.com)
* [Ask for another board](https://github.com/Koheron/zynq-sdk/issues/new)

## Available instruments

* [`oscillo`](https://github.com/Koheron/zynq-sdk/tree/master/projects/oscillo) : simple signal acquisition / generation with coherent averaging mode.
* [`spectrum`](https://github.com/Koheron/zynq-sdk/tree/master/projects/spectrum) : spectrum analyzer with peak-detection and averaging.

## Quick start

You can find the latest release of the SD card image for the Red Pitaya `oscillo-<version>.img` on this [link](https://github.com/Koheron/zynq-sdk/releases). The `oscillo` and `spectrum` instruments are preinstalled.

1. [Connect the board](http://www.koheron.com/products/laser-development-kit/getting-started/) to your computer.
2. Navigate to your board ip address (e.g. `192.168.1.100`) with your browser.

## Build your own instrument

The build is tested on Ubuntu 16.04.
[Install Vivado 2016.1](https://github.com/Koheron/zynq-sdk/issues/101) and source it (`source scripts/settings.sh`):

Build zip file including the FPGA bitstream and its corresponding server:
```
$ make NAME=<project> zip
```

Run the project on your Zynq board and test it:
```
$ make NAME=<project> HOST=192.168.1.100 run test
```

## Build SD card image

Build zip, boot-loader and Linux kernel, then Ubuntu root file system:
```
$ make NAME=<project>
$ sudo bash scripts/image.sh <project>
```
