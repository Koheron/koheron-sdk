#!/bin/bash
set -e

. settings.sh

make NAME=led_blinker linux
make NAME=oscillo
make NAME=spectrum
make NAME=decimator
make NAME=laser_controller
make NAME=pulse_generator
make NAME=adc_dac
make NAME=cluster
