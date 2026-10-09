#!/usr/bin/env bash
set -euo pipefail
repo=$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)
vivado_dir=${VIVADO_PATH:-/tools/Xilinx/${VIVADO_VERSION:-2025.1}/Vivado}
if ! command -v xvlog >/dev/null; then
  # Xilinx setup scripts reference optional, unset environment variables.
  set +u
  source "$vivado_dir/settings64.sh"
  set -u
fi
out="$repo/tmp/tests/alpha250-adc-dac-dma/dac-buffer"
mkdir -p "$out"
cd "$out"
core="$repo/examples/alpha250/adc-dac-dma/dac_output_buffer_v1_0"
xvlog --sv "$core/dac_output_buffer.v" "$core/dac_output_buffer_tb.sv" > compile.log 2>&1
xvlog "$vivado_dir/data/verilog/src/glbl.v" >> compile.log 2>&1
for spec in '4.0 0.0' '4.0 0.5' '4.0 1.0' '5.0 0.0' '5.0 1.25'; do
  read -r period phase <<< "$spec"
  snapshot="dac_${period/./_}_${phase/./_}"
  xelab --generic_top "PERIOD_NS=$period" --generic_top "PHASE_NS=$phase" \
    dac_output_buffer_tb glbl -L xpm -L unisims_ver -s "$snapshot" > "$snapshot-elaborate.log" 2>&1
  xsim "$snapshot" --runall --onfinish quit --onerror quit > "$snapshot-run.log" 2>&1
  # XSim may finish successfully after $fatal; require the success marker too.
  grep 'PASS:' "$snapshot-run.log"
done
