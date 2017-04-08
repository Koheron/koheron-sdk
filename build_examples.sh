#!/bin/bash
set -e

target=$1
mode=production

make CONFIG=instruments/led-blinker/config.yml MODE=$mode $target
make CONFIG=instruments/oscillo/config.yml MODE=$mode $target
make CONFIG=instruments/spectrum/config.yml MODE=$mode $target
make CONFIG=instruments/decimator/config.yml MODE=$mode $target
make CONFIG=instruments/laser-controller/config.yml MODE=$mode $target
make CONFIG=instruments/pulse-generator/config.yml MODE=$mode $target
make CONFIG=instruments/adc-dac/config.yml MODE=$mode $target
make CONFIG=instruments/cluster/config.yml MODE=$mode $target
