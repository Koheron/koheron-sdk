#!/bin/bash
# set -e

target=$1
mode=development

make CONFIG=examples/alpha250/fft/config.yml MODE=$mode $target
make CONFIG=examples/alpha250/loopback/config.yml MODE=$mode $target
make CONFIG=examples/alpha250/adc-dac-bram/config.yml MODE=$mode $target
make CONFIG=examples/alpha250/adc-dac-dma/config.yml MODE=$mode $target
make CONFIG=examples/alpha250/phase-noise-analyzer/config.yml MODE=$mode $target
dmesg
make CONFIG=examples/red-pitaya/led-blinker/config.yml MODE=$mode $target
make CONFIG=examples/red-pitaya/oscillo/config.yml MODE=$mode $target
make CONFIG=examples/red-pitaya/spectrum/config.yml MODE=$mode $target
make CONFIG=examples/red-pitaya/decimator/config.yml MODE=$mode $target
make CONFIG=examples/red-pitaya/laser-controller/config.yml MODE=$mode $target
make CONFIG=examples/red-pitaya/pulse-generator/config.yml MODE=$mode $target
make CONFIG=examples/red-pitaya/adc-dac/config.yml MODE=$mode $target
make CONFIG=examples/red-pitaya/cluster/config.yml MODE=$mode $target
make CONFIG=examples/red-pitaya/dual-dds/config.yml MODE=$mode $target
make CONFIG=examples/red-pitaya/fft/config.yml MODE=$mode $target
make CONFIG=examples/red-pitaya/phase-noise-analyzer/config.yml MODE=$mode $target
make CONFIG=examples/zedboard/led-blinker/config.yml MODE=$mode $target
make CONFIG=examples/zedboard/picoblaze/config.yml MODE=$mode $target
