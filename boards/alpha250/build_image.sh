#!/usr/bin/env bash
set -euo pipefail

# make CFG=examples/alpha250/phase-noise-analyzer/config.mk clean_server
# make CFG=examples/alpha250/adc-dac-bram/config.mk clean_server
# make CFG=examples/alpha250/adc-dac-dma/config.mk clean_server
# make CFG=examples/alpha250/fft/config.mk clean_server

make -j CFG=examples/alpha250/phase-noise-analyzer/config.mk
make -j CFG=examples/alpha250/adc-dac-bram/config.mk
make -j CFG=examples/alpha250/adc-dac-dma/config.mk
make -j CFG=examples/alpha250/fft/config.mk \
  COPY_INSTRUMENTS="phase-noise-analyzer adc-dac-bram adc-dac-dma" image

# Optional: cache sudo now so it doesn’t prompt at the very end
sudo -v

# Only this runs as root; preserve PATH in case your toolchain is in it
sudo --preserve-env=PATH make CFG=examples/alpha250/fft/config.mk flash