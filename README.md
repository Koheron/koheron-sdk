# zynq-sdk

#### `Software Development Kit for Zynq-based Systems`

[![Build Status](http://3c60fce9.ngrok.io/job/zynq-sdk/badge/icon)](http://3c60fce9.ngrok.io/job/zynq-sdk/)

This project aims to bring together the best practices of software development on the Zynq platform.

## Objectives

* Simple workflow for FPGA prototyping
* Easy setup of modern Linux distributions
* Fast and simple remote control using scripting languages

### Boards

* [Red Pitaya](http://redpitaya.com)

### Linux distributions

* Ubuntu 14.04

### Scripting languages

* Python
* Javascript

## Usage

[Install Vivado](https://github.com/Koheron/zynq-sdk/issues/37)

```
$ source /opt/Xilinx/Vivado/2015.4/settings64.sh
```

[Full list of requirements](https://github.com/Koheron/zynq-sdk/issues/4)

Builds `oscillo` bitstream and Linux kernel:
```
$ make
```

Build Ubuntu image:
```
$ sudo bash scripts/image.sh scripts/ubuntu.sh oscillo 1024
```

Build the bitstream `spectrum`:
```
$ make tmp/spectrum.bit
```
