# koheron-sdk

[![CircleCI](https://dl.circleci.com/status-badge/img/gh/Koheron/koheron-sdk/tree/master.svg?style=svg)](https://dl.circleci.com/status-badge/redirect/gh/Koheron/koheron-sdk/tree/master)
[![PyPI version](https://img.shields.io/pypi/v/koheron.svg)](https://pypi.python.org/pypi/koheron)

https://www.koheron.com/software-development-kit

## Getting started

The SDK is tested on an Ubuntu 22.04 development machine.

1. Install Vivado. Instruments can be built on Vivado versions newer than 2017.2. The OS can only be built with Vivado 2017.2. The branch [V1](https://github.com/Koheron/koheron-sdk/tree/V1) uses Vivado/Vitis 2025.1 and is not backward compatible with V0.x.

2. Install required packages

    ```bash
    $ make setup
    ```

3. Install Ubuntu 22.04 for Zynq ([Download SD card image](https://www.koheron.com/software-development-kit/documentation/ubuntu-zynq/))

4. Build and run an instrument

    ```bash
    $ make CONFIG=examples/alpha250/adc-dac-bram/config.yml HOST=192.168.1.100 run
    $ HOST=192.168.1.100 python3 examples/alpha250/adc-dac-bram/test.py
    ```

Ready to develop your instrument? Read the [documentation](https://www.koheron.com/software-development-kit/documentation).

## Koheron Alpha250 designs

* [`fft`](https://github.com/Koheron/koheron-sdk/tree/master/examples/alpha250/fft) : reference design with spectrum analyzer, DDS and demodulation.
* [`phase-noise-analyzer`](https://github.com/Koheron/koheron-sdk/tree/master/examples/alpha250/adc-dac-dma) : phase noise analyzer.
* [`loopback`](https://github.com/Koheron/koheron-sdk/tree/master/examples/alpha250/loopback) : minimal instrument.
* [`adc-dac-bram`](https://github.com/Koheron/koheron-sdk/tree/master/examples/alpha250/adc-dac-bram) : set DAC waveforms and get ADC using Block RAMs.
* [`adc-dac-dma`](https://github.com/Koheron/koheron-sdk/tree/master/examples/alpha250/adc-dac-dma) : set DAC waveforms and get ADC using DMA.

## Red Pitaya designs

* [`adc-dac`](https://github.com/Koheron/koheron-sdk/tree/master/examples/red-pitaya/adc-dac) : instrument with minimal read/write capability on Red Pitaya ADCs and DACs.
* [`decimator`](https://github.com/Koheron/koheron-sdk/tree/master/examples/red-pitaya/decimator) : decimation using a compensated CIC filter.

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
