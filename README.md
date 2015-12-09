# zynq-sdk

#### `Software Development Kit for Zynq-based Systems`

This project aims to bring together the best practices of software development on the Zynq platform.

## Objectives

* Simple workflow for FPGA prototyping
* Easy setup of modern Linux distributions
* Fast and simple remote control using scripting languages

### Boards

* [Red Pitaya](http://redpitaya.com)

### Linux distributions

* Ubuntu

### Scripting languages

* Python
* Javascript

## Usage

[Install Vivado](https://github.com/Koheron/zynq-sdk/issues/37)

```
$ source /opt/Xilinx/Vivado/2015.4/settings64.sh
$ source /opt/Xilinx/SDK/2015.4/settings64.sh
```

Builds `oscillo` bitstream and Linux kernel:
```
$ make
```

Build Ubuntu image:
```
$ sudo bash scripts/image.sh scripts/ubuntu.sh oscillo.img 1024
```

Build the bitstream `spectrum`:
```
$ make tmp/spectrum.bit
```
