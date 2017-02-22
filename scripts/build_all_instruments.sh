#!/bin/bash
set -e

. settings.sh
mode=production

make NAME=led_blinker MODE=$mode linux
make NAME=oscillo MODE=$mode
make NAME=spectrum MODE=$mode
make NAME=decimator MODE=$mode
make NAME=laser_controller MODE=$mode
make NAME=pulse_generator MODE=$mode
make NAME=adc_dac MODE=$mode
make NAME=cluster MODE=$mode
