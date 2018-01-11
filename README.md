# koheron-sdk

[![Circle CI](https://circleci.com/gh/Koheron/koheron-sdk.svg?style=shield)](https://circleci.com/gh/Koheron/koheron-sdk)
[![PyPI version](https://img.shields.io/pypi/v/koheron.svg)](https://pypi.python.org/pypi/koheron)

## What is Koheron Software Development Kit ?

[Koheron Software Development Kit](https://www.koheron.com/software-development-kit) is a tool to develop FPGA / Linux instruments for Zynq SoCs.

## Getting started

1. [Install Vivado 2017.2](https://koheron.com/software-development-kit/documentation/setup-development-machine)

2. Install required packages

    ```bash
    $ make setup
    ```

3. Install Ubuntu 16.04 for Zynq ([Download SD card image](https://www.koheron.com/software-development-kit/documentation/ubuntu-zynq/))

4. Build and run the led-blinker instrument

    ```bash
    $ make CONFIG=examples/zedboard/led-blinker/config.yml HOST=192.168.1.100 run
    ```

Ready to develop your instrument? Read the [documentation](https://www.koheron.com/software-development-kit/documentation).

## Koheron Alpha250 designs

* [`fft`](https://github.com/Koheron/koheron-sdk/tree/master/examples/alpha250/fft) : reference design with spectrum analyzer, DDS and demodulation.
* [`loopback`](https://github.com/Koheron/koheron-sdk/tree/master/examples/alpha250/loopback) : minimal instrument.
* [`adc-dac-bram`](https://github.com/Koheron/koheron-sdk/tree/master/examples/alpha250/adc-dac-bram) : set DAC waveforms and get ADC using Block RAMs.
* [`adc-dac-dma`](https://github.com/Koheron/koheron-sdk/tree/master/examples/alpha250/adc-dac-dma) : set DAC waveforms and get ADC using DMA.

## Red Pitaya designs

* [`led-blinker`](https://github.com/Koheron/koheron-sdk/tree/master/examples/red-pitaya/led-blinker) : minimal instrument with LED control.
* [`adc-dac`](https://github.com/Koheron/koheron-sdk/tree/master/examples/red-pitaya/adc-dac) : instrument with minimal read/write capability on Red Pitaya ADCs and DACs.
* [`pulse-generator`](https://github.com/Koheron/koheron-sdk/tree/master/examples/red-pitaya/pulse-generator) : pulse generation with synchronous acquisition.
* [`laser-controller`](https://github.com/Koheron/koheron-sdk/tree/master/examples/red-pitaya/laser-controller) : laser current control using pulse-density modulation.
* [`decimator`](https://github.com/Koheron/koheron-sdk/tree/master/examples/red-pitaya/decimator) : decimation using a compensated CIC filter.
* [`oscillo`](https://github.com/Koheron/koheron-sdk/tree/master/examples/red-pitaya/oscillo) : signal acquisition / generation with coherent averaging mode.
* [`spectrum`](https://github.com/Koheron/koheron-sdk/tree/master/examples/red-pitaya/spectrum) : spectrum analyzer with peak-detection and averaging.

## Zedboard designs

* [`led-blinker`](https://github.com/Koheron/koheron-sdk/tree/master/examples/zedboard/led-blinker) : minimal instrument with LED control.
* [`picoblaze`](https://github.com/Koheron/koheron-sdk/tree/master/examples/zedboard/picoblaze) : 8 bit picoblaze microcontroller controllable from the PS.

## How to

Build an instrument:
```
$ make CONFIG=path/to/config.yml
```

Build an instrument block design:
```
$ make CONFIG=path/to/config.yml block_design
```

More commands are listed in the [documentation](https://www.koheron.com/software-development-kit/documentation/build-run-makefile).

## Acknowledgments

This project started as a fork of [red-pitaya-notes](https://github.com/pavel-demin/red-pitaya-notes).
