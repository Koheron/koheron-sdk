# zynq-sdk

[![Circle CI](https://circleci.com/gh/Koheron/zynq-sdk.svg?style=shield)](https://circleci.com/gh/Koheron/zynq-sdk)

#### `Software Development Kit for Zynq-based instruments`

### Goals

* YAML-based instrument configuration
* IP-centric workflow for Hardware / FPGA prototyping
* [tcp-server](https://github.com/Koheron/tcp-server) for direct access of mapped memory
* Python and Javascript APIs
* Continuous Delivery pipeline

###  Supported Base Boards

* [Red Pitaya](http://redpitaya.com)
* [Ask for another board](https://github.com/Koheron/zynq-sdk/issues/new)

## Quick start

You can find the latest release of the SD card image `oscillo-<version>.img` on this [link](https://github.com/Koheron/zynq-sdk/releases). The image contains an Ubuntu distribution with the `oscillo` and `spectrum` projects preinstalled.

* [Connect the board](http://www.koheron.com/products/lase/getting-started/) to your computer (Windows and Linux).

## Build your own image

The build is tested on Ubuntu 14.04.
[Install Vivado](https://github.com/Koheron/zynq-sdk/issues/37) and source it ([Full list of requirements](https://github.com/Koheron/zynq-sdk/issues/4)):
```
$ source settings.sh
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

## Start an instrument project

Instrument configuration is done via a YAML file:

```yaml
---
project: blink
parent: default
board: red-pitaya

cores:
  - redp_adc_v1_0
  - redp_dac_v1_0
  - axi_cfg_register_v1_0
  - axi_sts_register_v1_0

addresses:
  - name: config
    offset: '0x60000000'
    range: 4K
  - name: status
    offset: '0x50000000'
    range: 4K

config_offsets:
  - led
  - dac1
  - dac2

status_offsets:
  - adc1
  - adc2

parameters:
  bram_addr_width: 13
  dac_width: 14
  adc_width: 14
  pwm_width: 10
  n_pwm: 4
  config_size: 16
  status_size: 16

xdc:
  - boards/red-pitaya/config/ports.xdc
  - boards/red-pitaya/config/clocks.xdc
```

