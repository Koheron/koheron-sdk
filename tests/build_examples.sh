#!/bin/bash
set -e

target=server
mode=development

make CFG=examples/alpha250/fft/config.mk -j $target
make CFG=examples/alpha250/loopback/config.mk -j $target
make CFG=examples/alpha250/adc-dac-bram/config.mk -j $target
make CFG=examples/alpha250/adc-dac-dma/config.mk -j $target
make CFG=examples/alpha250/dpll/config.mk -j $target
make CFG=examples/alpha250/phase-noise-analyzer/config.mk -j $target
make CFG=examples/alpha250-4/adc-bram/config.mk N_CPUS=1 -j $target
make CFG=examples/alpha250-4/fft/config.mk -j $target
make CFG=examples/red-pitaya/led-blinker/config.mk -j $target
make CFG=examples/red-pitaya/oscillo/config.mk -j $target
make CFG=examples/red-pitaya/spectrum/config.mk -j $target
make CFG=examples/red-pitaya/decimator/config.mk -j $target
make CFG=examples/red-pitaya/laser-controller/config.mk -j $target
make CFG=examples/red-pitaya/pulse-generator/config.mk -j $target
make CFG=examples/red-pitaya/adc-dac/config.mk -j $target
make CFG=examples/red-pitaya/cluster/config.mk -j $target
make CFG=examples/red-pitaya/dual-dds/config.mk -j $target
make CFG=examples/red-pitaya/fft/config.mk -j $target
make CFG=examples/red-pitaya/phase-noise-analyzer/config.mk -j $target