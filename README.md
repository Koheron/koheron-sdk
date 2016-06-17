# zynq-sdk

[![Circle CI](https://circleci.com/gh/Koheron/zynq-sdk.svg?style=shield)](https://circleci.com/gh/Koheron/zynq-sdk)
[![Code Climate](https://codeclimate.com/github/Koheron/zynq-sdk/badges/gpa.svg)](https://codeclimate.com/github/Koheron/zynq-sdk)

#### `Software Development Kit for Zynq-based instruments`

* Ubuntu Core 16.04 LTS
* Vivado 2016.1 toolchain (Linux Kernel version 4.4)
* User-space control of hardware with [TCP / Websocket server](https://github.com/Koheron/tcp-server)
* HTTP, Python and Javascript APIs
* Compatible with Docker

### Supported Base Boards

* [Red Pitaya](http://redpitaya.com)
* [Ask for another board](https://github.com/Koheron/zynq-sdk/issues/new)

## Available instruments

* [`oscillo`](https://github.com/Koheron/zynq-sdk/tree/master/projects/oscillo) : simple signal acquisition / generation with coherent averaging mode.
* [`spectrum`](https://github.com/Koheron/zynq-sdk/tree/master/projects/spectrum) : spectrum analyzer with peak-detection and averaging.

## Quick start

You can find the latest release of the SD card image for the Red Pitaya `oscillo-<version>.img` on this [link](https://github.com/Koheron/zynq-sdk/releases). The `oscillo` and `spectrum` instruments are preinstalled.

* [Connect the board](http://www.koheron.com/products/laser-development-kit/getting-started/) to your computer (Windows and Linux).

## Build your own image

The build is tested on Ubuntu 16.04.
[Install Vivado 2016.1](https://github.com/Koheron/zynq-sdk/issues/101) and source it ([Full list of requirements](https://github.com/Koheron/zynq-sdk/issues/117)):
```
$ source scripts/settings.sh
```

Build zip file including bitstream, middleware and python drivers:
```
$ make NAME=<project> zip
```

Build zip, boot-loader and Linux kernel:
```
$ make NAME=<project>
```

Build Ubuntu SD card image:
```
$ sudo bash scripts/image.sh <project>
```
