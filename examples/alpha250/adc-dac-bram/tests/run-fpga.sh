#!/usr/bin/env bash
set -euo pipefail
repo=$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)
vivado_dir=${VIVADO_PATH:-/tools/Xilinx/${VIVADO_VERSION:-2025.1}/Vivado}
if ! command -v xvlog >/dev/null; then
  set +u
  source "$vivado_dir/settings64.sh"
  set -u
fi
out="$repo/tmp/tests/alpha250-adc-dac-bram/capture-completion"
mkdir -p "$out"
cd "$out"
core="$repo/examples/alpha250/adc-dac-bram/address_counter_v1_0/address_counter.v"
tb="$repo/examples/alpha250/adc-dac-bram/tests/capture_completion_tb.sv"
xvlog --sv "$core" "$tb" > compile.log 2>&1
for width in 5 9 16; do
  for period in 4.0 5.0; do
    snapshot="capture_${width}_${period/./_}"
    xelab --generic_top "COUNT_WIDTH=$width" --generic_top "PERIOD_NS=$period" \
      capture_completion_tb -s "$snapshot" > "$snapshot-elaborate.log" 2>&1
    xsim "$snapshot" --runall --onfinish quit --onerror quit > "$snapshot-run.log" 2>&1
    rg 'PASS:' "$snapshot-run.log"
  done
done
