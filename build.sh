#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
BOARDS_DIR="${ROOT_DIR}/boards"

log() {
  printf '\n\033[1;34m[INFO]\033[0m %s\n' "$1"
}

success() {
  printf '\n\033[1;32m[SUCCESS]\033[0m %s\n' "$1"
}

failure() {
  printf '\n\033[1;31m[FAILED]\033[0m %s\n' "$1"
}

print_usage() {
  cat <<USAGE
Usage: $0 <board>|--list|--all

Examples:
  $0 alpha250
  $0 --all
  $0 --list
  $0               # interactive mode
USAGE
}

list_available_boards() {
  find "${BOARDS_DIR}" -mindepth 1 -maxdepth 1 -type d -print0 \
    | sort -z \
    | while IFS= read -r -d '' board_dir; do
        if [[ -f "${board_dir}/build_image.conf" ]]; then
          basename "${board_dir}"
        fi
      done
}

select_board_interactively() {
  mapfile -t boards < <(list_available_boards)
  if ((${#boards[@]} == 0)); then
    printf 'No boards with build_image.conf found in %s\n' "${BOARDS_DIR}" >&2
    exit 1
  fi

  printf 'Available boards:\n' >&2
  printf '   0) All boards\n' >&2
  for i in "${!boards[@]}"; do
    printf '  %2d) %s\n' "$((i + 1))" "${boards[$i]}" >&2
  done
  printf '\nSelect a board by number (or press Enter to cancel): ' >&2
  read -r selection
  if [[ -z "${selection}" ]]; then
    exit 0
  fi
  if ! [[ ${selection} =~ ^[0-9]+$ ]]; then
    printf 'Invalid selection: %s\n' "${selection}" >&2
    exit 1
  fi
  if ((selection == 0)); then
    printf '__ALL__'
    return
  fi
  local index=$((selection - 1))
  if ((index < 0 || index >= ${#boards[@]})); then
    printf 'Selection out of range: %s\n' "${selection}" >&2
    exit 1
  fi
  printf '%s' "${boards[$index]}"
}

build_all_boards() {
  mapfile -t boards < <(list_available_boards)
  if ((${#boards[@]} == 0)); then
    printf 'No boards with build_image.conf found in %s\n' "${BOARDS_DIR}" >&2
    exit 1
  fi

  for board in "${boards[@]}"; do
    log "Starting build for ${board}"
    build_board "${board}" false
  done
}

load_board_config() {
  local board="$1"
  local config_file="${BOARDS_DIR}/${board}/build_image.conf"
  if [[ ! -f "${config_file}" ]]; then
    printf 'No build_image.conf found for board %s at %s\n' "${board}" "${config_file}" >&2
    exit 1
  fi

  unset BOARD_NAME IMAGE_CONFIG
  INSTRUMENT_CONFIGS=()
  COPY_INSTRUMENTS=()

  # shellcheck disable=SC1090
  source "${config_file}"

  : "${BOARD_NAME:?BOARD_NAME must be set in ${config_file}}"
  : "${IMAGE_CONFIG:?IMAGE_CONFIG must be set in ${config_file}}"
}

run_make_command() {
  log "$*"
  "$@"
}

build_board() {
  local board="$1"
  local prompt_flash=${2:-true}
  load_board_config "${board}"

  SECONDS=0
  trap 'failure "Build failed after ${SECONDS}s."' ERR

  log "Preparing build for ${BOARD_NAME}."
  if ((${#INSTRUMENT_CONFIGS[@]} > 0)); then
    printf '  • Instrument configs: %s\n' "${INSTRUMENT_CONFIGS[*]}"
  else
    printf '  • Instrument configs: (none)\n'
  fi
  printf '  • Image config:      %s\n' "${IMAGE_CONFIG}"
  if ((${#COPY_INSTRUMENTS[@]} > 0)); then
    printf '  • Copy instruments:  %s\n' "${COPY_INSTRUMENTS[*]}"
  else
    printf '  • Copy instruments:  (none)\n'
  fi

  for cfg in "${INSTRUMENT_CONFIGS[@]}"; do
    log "Building instrument for CFG=${cfg}"
    run_make_command make -j CFG="${cfg}"
  done

  if ((${#COPY_INSTRUMENTS[@]} > 0)); then
    log "Building SD card image with instruments: ${COPY_INSTRUMENTS[*]}"
    run_make_command make -j CFG="${IMAGE_CONFIG}" COPY_INSTRUMENTS="${COPY_INSTRUMENTS[*]}" image
  else
    log "Building SD card image"
    run_make_command make -j CFG="${IMAGE_CONFIG}" image
  fi

  if [[ ${prompt_flash} == true ]]; then
    read -r -p $'\nWould you like to burn the SD card image now? [y/N] ' answer
    answer=${answer,,}
    if [[ ${answer} == "y" || ${answer} == "yes" ]]; then
      log "Preparing to flash SD card. Caching sudo credentials..."
      sudo -v
      log "Flashing SD card using CFG=${IMAGE_CONFIG}"
      sudo --preserve-env=PATH make CFG="${IMAGE_CONFIG}" flash
      success "SD card image flashed successfully."
    else
      log "Skipping SD card flashing as requested."
    fi
  fi

  local duration=${SECONDS}
  trap - ERR
  local minutes=$((duration / 60))
  local seconds=$((duration % 60))
  success "Build completed in ${minutes}m ${seconds}s."
}

main() {
  if [[ $# -eq 0 ]]; then
    local selected_board
    selected_board=$(select_board_interactively)
    if [[ -z "${selected_board}" ]]; then
      exit 0
    fi
    if [[ ${selected_board} == "__ALL__" ]]; then
      build_all_boards
      exit 0
    fi
    build_board "${selected_board}" true
    exit 0
  fi

  case "$1" in
    --help|-h)
      print_usage
      ;;
    --all)
      build_all_boards
      ;;
    --list)
      list_available_boards
      ;;
    *)
      if [[ $1 == "all" ]]; then
        build_all_boards
      else
        build_board "$1" true
      fi
      ;;
  esac
}

main "$@"
