#!/bin/bash
set -e

target=$1
mode=development

make CONFIG=examples/led-blinker/config.yml MODE=$mode $target
make CONFIG=examples/oscillo/config.yml MODE=$mode $target
make CONFIG=examples/spectrum/config.yml MODE=$mode $target
make CONFIG=examples/decimator/config.yml MODE=$mode $target
make CONFIG=examples/laser-controller/config.yml MODE=$mode $target
make CONFIG=examples/pulse-generator/config.yml MODE=$mode $target
make CONFIG=examples/adc-dac/config.yml MODE=$mode $target
make CONFIG=examples/cluster/config.yml MODE=$mode $target
