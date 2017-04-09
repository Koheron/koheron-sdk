# koheron-sdk

[![CircleCI](https://circleci.com/gh/Koheron/ZynqSDK.svg?style=shield&circle-token=8bad0d504d47f62082ff5d8b07dfb21aaf03ecde)](https://circleci.com/gh/Koheron/ZynqSDK)

## What is Koheron SDK ?

Koheron SDK is a build system for quick prototyping of custom instruments on Zynq SoCs.

## Quickstart with the [Red Pitaya](http://redpitaya.com)

#### 1.1. Install Vivado 2016.4

#### 1.2 Install required packages

```bash
$ sudo make setup
```

### 2. Install Koheron Linux for Red Pitaya ([Download SD card image](https://github.com/Koheron/koheron-sdk/releases))

### 3. Build and run the `led-blinker` instrument

```bash
$ make CONFIG=examples/led-blinker/config.yml HOST=192.168.1.100 run
```
where `HOST` is your Red Pitaya IP address.

## Examples of instruments

* [`led_blinker`](https://github.com/Koheron/koheron-sdk/tree/master/examples/led-blinker) : minimal instrument with LED control from Python.
* [`adc_dac`](https://github.com/Koheron/koheron-sdk/tree/master/examples/adc-dac) : instrument with minimal read/write capability on Red Pitaya ADCs and DACs.
* [`pulse_generator`](https://github.com/Koheron/koheron-sdk/tree/master/examples/pulse-generator) : pulse generation with synchronous acquisition.
* [`laser_controller`](https://github.com/Koheron/koheron-sdk/tree/master/examples/decimator) : laser current control using pulse-density modulation.
* [`decimator`](https://github.com/Koheron/koheron-sdk/tree/master/examples/decimator) : decimation using a compensated CIC filter.
* [`oscillo`](https://github.com/Koheron/koheron-sdk/tree/master/examples/oscillo) : signal acquisition / generation with coherent averaging mode.
* [`spectrum`](https://github.com/Koheron/koheron-sdk/tree/master/examples/spectrum) : spectrum analyzer with peak-detection and averaging.

## How to

Open Vivado and build the instrument block design:
```
$ make CONFIG=path/to/config.yml bd
```

Build the SD card image:
```
$ make linux
$ sudo make image
```

Build the instrument (without running it):
```
$ make CONFIG=path/to/config.yml
```

Test a verilog core:
```
$ make CORE=fpga/cores/comparator_v1_0 test_core
```

Test a Tcl module:
```
$ make CONFIG=path/to/config.yml test_module
```

## Acknowledgments

This project started as a fork of [red-pitaya-notes](https://github.com/pavel-demin/red-pitaya-notes).