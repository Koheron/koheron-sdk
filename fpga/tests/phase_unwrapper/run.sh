#!/usr/bin/env bash
set -euo pipefail

# Set VIVADO_SETTINGS to a settings64.sh path, or use tools already in PATH.
if [[ -n "${VIVADO_SETTINGS:-}" ]]; then
    set +u
    source "$VIVADO_SETTINGS"
    set -u
fi
repo=$(cd "$(dirname "$0")/../../.." && pwd)
out="$repo/tmp/tests/phase-unwrapper"
mkdir -p "$out"
cd "$out"

xvlog --sv "$repo/fpga/cores/phase_unwrapper_v1_0/phase_unwrapper.v" \
    "$repo/fpga/tests/phase_unwrapper/test_lookahead.sv" \
    "$repo/fpga/tests/phase_unwrapper/test_full_history.sv" \
    "$repo/fpga/tests/phase_unwrapper/test_latency.sv" \
    "$repo/fpga/cores/phase_range_guard_v1_0/phase_range_guard.v" \
    "$repo/examples/alpha250-4/phase-noise-analyzer/tests/test_phase_headroom.sv" \
    "$repo/examples/alpha250-4/phase-noise-analyzer/tests/test_phase_unwrapper_pipeline.sv" \
    "$repo/examples/alpha250/phase-noise-analyzer/tests/test_phase_history_tb.sv" \
    "$repo/examples/alpha250/dpll/tests/test_phase_history_tb.sv" \
    "$XILINX_VIVADO/data/verilog/src/glbl.v"
for test in test_lookahead test_full_history test_latency test_phase_headroom test_phase_unwrapper_pipeline test_pna_phase_history_tb test_phase_history_tb; do
    xelab -L unisims_ver "work.$test" work.glbl -s "$test" > "$test-elab.log" 2>&1
    xsim "$test" -runall > "$test.log" 2>&1
    cat "$test.log"
    if rg -q 'Fatal:|ERROR:|FATAL:' "$test.log"; then exit 1; fi
    rg -q 'PASS|passed' "$test.log"
done
