# koheron-sdk

[![Circle CI](https://circleci.com/gh/Koheron/koheron-sdk.svg?style=shield)](https://circleci.com/gh/Koheron/koheron-sdk)

## What is Koheron SDK ?

Koheron SDK is a build system for quick prototyping of custom instruments on Zynq SoCs.

## Quickstart with the [Red Pitaya](http://redpitaya.com)

### 1. Requirements for Ubuntu 16.04

Download [`Vivado HLx 2016.2: All OS Installer Single-File Download`](http://www.xilinx.com/support/download/index.html/content/xilinx/en/downloadNav/vivado-design-tools/2016-2.html).

```bash
$ sudo apt-get install curl
$ cd ~/Downloads
$ sudo curl https://raw.githubusercontent.com/Koheron/koheron-sdk/master/scripts/install_vivado.sh | /bin/bash /dev/stdin
$ sudo ln -s make /usr/bin/gmake # tells Vivado to use make instead of gmake
```

Install requirements:

```bash
$sudo apt-get install git curl zip python-virtualenv python-pip \
    g++-arm-linux-gnueabihf lib32stdc++6 lib32z1 u-boot-tools\
    libssl-dev bc device-tree-compiler qemu-user-static
$ sudo apt-get install git
$ git clone https://github.com/Koheron/koheron-sdk
$ cd koheron-sdk
$ sudo pip install -r requirements.txt
```

### 2. Install Koheron Linux for Red Pitaya ([Download SD card image](https://github.com/Koheron/koheron-sdk/releases))

### 3. Build and run the minimal instrument

```bash
$ source scripts/settings.sh
$ export HOST=192.168.1.100 # your Red Pitaya IP address
$ make NAME=blink run
```

### 4. Ping the board and watch the LEDs blink
```bash
$ curl http://$(HOST)/api/board/ping
```

## Examples of instruments

* [`blink`](https://github.com/Koheron/koheron-sdk/tree/master/projects/blink) : minimal instrument with access to LEDs and Red Pitaya ADCs and DACs.
* [`oscillo`](https://github.com/Koheron/koheron-sdk/tree/master/projects/oscillo) : signal acquisition / generation with coherent averaging mode.
* [`spectrum`](https://github.com/Koheron/koheron-sdk/tree/master/projects/spectrum) : spectrum analyzer with peak-detection and averaging.

## How to

Open Vivado and build the instrument block design:
```
$ make NAME=oscillo bd
```

Build the SD card image:
```
$ make NAME=blink linux
$ sudo bash scripts/image.sh blink
```

Build the instrument (without running it):
```
$ make NAME=oscillo
```

Test a verilog core:
```
$ make CORE=comparator_v1_0 test_core
```

Test a Tcl module:
```
$ make NAME=averager test_module
```


