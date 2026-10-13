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

Build board images and configured instruments without writing an SD card.

Examples:
  $0 alpha250
  $0 --all
  $0 --list
  $0               # interactive mode

Set N_CPUS to a positive integer to bound make jobs and tool threads.
USAGE
}

list_board_configs() {
  find "${BOARDS_DIR}" -mindepth 2 -maxdepth 3 -type f -name build_image.conf -print0 \
    | sort -z
}

board_target() (
  local config_file="$1"
  unset BOARD_TARGET
  # Nested board packages declare their existing build target explicitly.
  # shellcheck disable=SC1090
  source "${config_file}"
  printf '%s\n' "${BOARD_TARGET:-$(basename "$(dirname "${config_file}")")}"
)

list_available_boards() {
  list_board_configs \
    | while IFS= read -r -d '' config_file; do
        board_target "${config_file}"
      done \
    | sort
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
    build_board "${board}"
  done
}

load_board_config() {
  local board="$1"
  local config_file="" candidate
  while IFS= read -r -d '' candidate; do
    if [[ $(board_target "${candidate}") == "${board}" ]]; then
      if [[ -n "${config_file}" ]]; then
        printf 'Multiple build_image.conf files declare board %s\n' "${board}" >&2
        exit 1
      fi
      config_file="${candidate}"
    fi
  done < <(list_board_configs)
  if [[ -z "${config_file}" ]]; then
    printf 'No build_image.conf found for board %s under %s\n' "${board}" "${BOARDS_DIR}" >&2
    exit 1
  fi

  unset BOARD_TARGET BOARD_NAME IMAGE_CONFIG
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
  load_board_config "${board}"
  local build_jobs=${N_CPUS:-$(nproc 2>/dev/null || printf '1')}
  if [[ ! $build_jobs =~ ^[1-9][0-9]*$ ]]; then
    printf 'N_CPUS must be a positive integer, got: %s\n' "$build_jobs" >&2
    exit 1
  fi

  SECONDS=0
  trap 'failure "Build failed after ${SECONDS}s."' ERR

  log "Preparing build for ${BOARD_NAME}."
  printf '  • Parallel jobs:      %s\n' "$build_jobs"
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
    run_make_command make -j"${build_jobs}" N_CPUS="${build_jobs}" CFG="${cfg}"
  done

  if ((${#COPY_INSTRUMENTS[@]} > 0)); then
    log "Building SD card image with instruments: ${COPY_INSTRUMENTS[*]}"
    run_make_command make -j"${build_jobs}" N_CPUS="${build_jobs}" CFG="${IMAGE_CONFIG}" COPY_INSTRUMENTS="${COPY_INSTRUMENTS[*]}" image
  else
    log "Building SD card image"
    run_make_command make -j"${build_jobs}" N_CPUS="${build_jobs}" CFG="${IMAGE_CONFIG}" image
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
    build_board "${selected_board}"
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
        build_board "$1"
      fi
      ;;
  esac
}

main "$@"
