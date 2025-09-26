# koheron-sdk

[![PyPI version](https://img.shields.io/pypi/v/koheron.svg)](https://pypi.python.org/pypi/koheron)

https://www.koheron.com/software-development-kit

## Getting started

The SDK is tested on an Ubuntu 24.04 development machine with Vivado/Vitis 2025.1.

1. Install Vivado/Vitis 2025.1.

2. Install required packages:

    ```bash
    $ make setup
    ```

3. Build the SD card image (Ubuntu 24.04.3 with xilinx-linux-v2025.1 kernel):

    ```bash
    $ make -j CFG=examples/alpha250/adc-dac-bram/config.mk image
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
$ make CFG=path/to/config.mk
```

Build an instrument block design:
```
$ make CFG=path/to/config.mk block_design
```

More commands are listed in the [documentation](https://www.koheron.com/software-development-kit/documentation/build-run-makefile).

## Acknowledgments

This project started as a fork of [red-pitaya-notes](https://github.com/pavel-demin/red-pitaya-notes).
