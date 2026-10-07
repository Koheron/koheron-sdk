#!/usr/bin/env bash
set -euo pipefail
if [[ -n ${DPLL_VIVADO_SETTINGS:-} ]]; then
    set +u
    source "$DPLL_VIVADO_SETTINGS"
    set -u
fi
here=$(cd "$(dirname "$0")" && pwd)
repo=$(cd "$here/../../../.." && pwd)
out=${DPLL_P_PATH_OUT:-"$repo/tmp/tests/alpha250-dpll/p-path"}
mkdir -p "$out"
cd "$out"
"${DPLL_TEST_PYTHON:-$repo/.venv/bin/python3}" "$here/p_detector_vectors.py" "$out/vectors.txt"
xvlog --sv "$XILINX_VIVADO/data/verilog/src/glbl.v" "$repo/fpga/cores/phase_unwrapper_v1_0/phase_unwrapper.v" "$here/test_phase_history_tb.sv" "$here/../fast_p_detector.v" "$here/../p_path_switch.v" \
    "$repo/fpga/cores/axi_ctl_register_v1_0/axi_ctl_register.v" \
    "$repo/fpga/cores/axi_sts_register_v1_0/axi_sts_register.v" \
    "$here/../table_gain.v" "$here/../table_corrector.v" "$here/../manual_p_corrector.v" \
    "$here/../accurate_phase_consumers.v" "$here/test_fast_pi_tb.sv" "$here/test_phase_consumers_tb.sv" \
    "$here/test_p_detector_tb.v" "$here/test_p_switch_tb.v" \
    "$here/test_manual_p_tb.v" "$here/test_p_axi_tb.v" > compile.log 2>&1
for top in test_p_detector_tb test_p_switch_tb test_manual_p_tb test_p_axi_tb test_fast_pi_tb test_phase_consumers_tb test_phase_history_tb; do
    xelab -L unisims_ver "work.$top" work.glbl -s "$top" > "$top-elaborate.log" 2>&1
    xsim "$top" -testplusarg "VECTORS=$out/vectors.txt" -runall > "$top.log" 2>&1
    rg 'checks passed|latency|Fatal:|ERROR:' "$top.log" || true
    rg -q 'checks passed' "$top.log"
    if rg -q 'Fatal:|ERROR:|FATAL:' "$top.log"; then exit 1; fi
done
cores=${DPLL_CORES:-"$repo/tmp/examples/alpha250/dpll/cores"}
vivado -mode batch -nolog -nojournal -notrace \
    -source "$here/test_p_frontend.tcl" -tclargs "$cores" "$out/frontend" > frontend.log 2>&1
rg 'checks passed|P latency|Fatal:|ERROR:' frontend.log || true
rg -q 'P front-end checks passed' frontend.log
# Reject stale packaged IP whose missing parameters Vivado otherwise ignores.
if rg -q 'Cannot set the parameter|Fatal:|ERROR:|FATAL:' frontend.log; then exit 1; fi
