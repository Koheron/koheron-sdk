# zynq-sdk

#### `Software Development Kit for Zynq-based instruments`

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
  spi_in: 0
  led: 1
  pwm0: 2
  pwm1: 3
  pwm2: 4
  pwm3: 5
  addr: 6
  avg0: 7
  avg1: 8
  
status_offsets: # [0-7] reserved for sha
  spi_out: 8 
  n_avg0: 9
  n_avg1: 10
  
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

[Install Vivado](https://github.com/Koheron/zynq-sdk/issues/37)

```
$ source /opt/Xilinx/Vivado/2015.4/settings64.sh
```

[Full list of requirements](https://github.com/Koheron/zynq-sdk/issues/4)

Builds `oscillo` bitstream and Linux kernel:
```
$ make NAME=oscillo
```

Build Ubuntu image:
```
$ sudo bash scripts/image.sh scripts/ubuntu.sh oscillo 1024
```
