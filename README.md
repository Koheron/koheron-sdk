# zynq-sdk

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

## Start an instrument project

Instrument configuration is done via a YAML file:

```yaml
---
project: oscillo

parent: base 

addresses:
  - name: config
    offset: '0x60000000'
    range: 4K
  - name: status
    offset: '0x50000000'
    range: 4K
  - name: dac
    offset: '0x40000000'
    range: 32K
  - name: adc1
    offset: '0x42000000'
    range: 32K
  - name: adc2
    offset: '0x44000000'
    range: 32K

config_offsets:
  - spi_in
  - led
  - pwm0
  - pwm1
  - pwm2
  - pwm3
  - addr
  - avg0
  - avg1
  
status_offsets:
  - spi_out
  - n_avg0
  - n_avg1
  
parameters:
  bram_addr_width: 13
  pwm_width: 10
  n_pwm: 4
  
devices:
  - oscillo.hpp
  - base/base.hpp
  - base/gpio.hpp
  - base/xadc.hpp
  - base/init.hpp

python_driver:
  name: Oscillo
  file: oscillo.py 

python:
  - oscillo.py
```


## Get started

The build is tested on Ubuntu 14.04.
* [Install Vivado](https://github.com/Koheron/zynq-sdk/issues/37) in `/opt/Xilinx`.
* [Full list of requirements](https://github.com/Koheron/zynq-sdk/issues/4)

Build zip file including bitstream, middleware and python drivers:
```
$ make NAME=<project> zip
```

Build bitstream, boot-loader and Linux kernel:
```
$ make NAME=<project>
```

Build Ubuntu image:
```
$ sudo bash scripts/image.sh scripts/ubuntu.sh <project> 1024
```
