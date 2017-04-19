# koheron-sdk

[![Circle CI](https://circleci.com/gh/Koheron/koheron-sdk.svg?style=shield)](https://circleci.com/gh/Koheron/koheron-sdk)
[![PyPI version](https://img.shields.io/pypi/v/koheron.svg)](https://pypi.python.org/pypi/koheron)

## What is Koheron Software Development Kit ?

[Koheron Software Development Kit](https://www.koheron.com/software-development-kit) is a tool to develop FPGA / Linux instruments for Zynq SoCs.

## Getting started with the Red Pitaya

1. [Install Vivado 2016.4](https://koheron.com/software-development-kit/documentation/setup-development-machine)

2. Install required packages

    ```bash
    $ sudo make setup
    ```

3. Install Koheron Linux for Red Pitaya ([Download SD card image](https://github.com/Koheron/koheron-sdk/releases))

4. Build and run the led-blinker instrument

    ```bash
    $ make CONFIG=examples/led-blinker/config.yml HOST=192.168.1.100 run
    ```

Ready to develop your instrument? Read the [documentation](https://www.koheron.com/software-development-kit/documentation).

## Examples

* [`led-blinker`](https://github.com/Koheron/koheron-sdk/tree/master/examples/led-blinker) : minimal instrument with LED control.
* [`adc-dac`](https://github.com/Koheron/koheron-sdk/tree/master/examples/adc-dac) : instrument with minimal read/write capability on Red Pitaya ADCs and DACs.
* [`pulse-generator`](https://github.com/Koheron/koheron-sdk/tree/master/examples/pulse-generator) : pulse generation with synchronous acquisition.
* [`laser-controller`](https://github.com/Koheron/koheron-sdk/tree/master/examples/decimator) : laser current control using pulse-density modulation.
* [`decimator`](https://github.com/Koheron/koheron-sdk/tree/master/examples/decimator) : decimation using a compensated CIC filter.
* [`oscillo`](https://github.com/Koheron/koheron-sdk/tree/master/examples/oscillo) : signal acquisition / generation with coherent averaging mode.
* [`spectrum`](https://github.com/Koheron/koheron-sdk/tree/master/examples/spectrum) : spectrum analyzer with peak-detection and averaging.

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