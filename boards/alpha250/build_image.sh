#!/usr/bin/env bash
set -euo pipefail

BOARD_NAME="Alpha250"
CONFIGS=(
  "examples/alpha250/phase-noise-analyzer/config.mk"
  "examples/alpha250/adc-dac-bram/config.mk"
  "examples/alpha250/adc-dac-dma/config.mk"
)
IMAGE_CFG="examples/alpha250/fft/config.mk"
COPY_INSTRUMENTS=(phase-noise-analyzer adc-dac-bram adc-dac-dma)

SECONDS=0

log() {
  printf '\n\033[1;34m[INFO]\033[0m %s\n' "$1"
}

success() {
  printf '\n\033[1;32m[SUCCESS]\033[0m %s\n' "$1"
}

failure() {
  printf '\n\033[1;31m[FAILED]\033[0m %s\n' "$1"
}

trap 'failure "Build failed after ${SECONDS}s."' ERR

log "Preparing build for ${BOARD_NAME}."
printf '  • Instrument configs: %s\n' "${CONFIGS[*]}"
printf '  • Image config:      %s\n' "${IMAGE_CFG}"
printf '  • Copy instruments:  %s\n' "${COPY_INSTRUMENTS[*]}"

for cfg in "${CONFIGS[@]}"; do
  log "Building instrument for CFG=${cfg}"
  make -j CFG="${cfg}"
done

log "Building SD card image with instruments: ${COPY_INSTRUMENTS[*]}"
make -j CFG="${IMAGE_CFG}" COPY_INSTRUMENTS="${COPY_INSTRUMENTS[*]}" image

read -r -p $'\nWould you like to burn the SD card image now? [y/N] ' answer
answer=${answer,,}

if [[ ${answer} == "y" || ${answer} == "yes" ]]; then
  log "Preparing to flash SD card. Caching sudo credentials..."
  sudo -v
  log "Flashing SD card using CFG=${IMAGE_CFG}"
  sudo --preserve-env=PATH make CFG="${IMAGE_CFG}" flash
  success "SD card image flashed successfully."
else
  log "Skipping SD card flashing as requested."
fi

duration=${SECONDS}
minutes=$((duration / 60))
seconds=$((duration % 60))

success "Build completed in ${minutes}m ${seconds}s."
