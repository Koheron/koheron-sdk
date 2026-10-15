#!/usr/bin/env bash
set -euo pipefail
if [[ -n ${DPLL_VIVADO_SETTINGS:-} ]]; then
    set +u
    source "$DPLL_VIVADO_SETTINGS"
    set -u
fi
here=$(cd "$(dirname "$0")" && pwd)
out=${DPLL_MULTIRATE_OUT:?Set DPLL_MULTIRATE_OUT to a scratch directory}
mkdir -p "$out"
cd "$out"
defines=()
if [[ -n ${DPLL_VERILOG_DEFINES:-} ]]; then defines=(--define "$DPLL_VERILOG_DEFINES"); fi
xvlog --sv "${defines[@]}" "$here/../table_gain.v" "$here/../table_corrector.v" \
    "$here/../fast_p_detector.v" "$here/../p_path_switch.v" "$here/../manual_p_corrector.v" \
    "$here/test_multirate_controller_tb.sv" "$XILINX_VIVADO/data/verilog/src/glbl.v"
xelab -L unisims_ver work.test_multirate_controller_tb work.glbl -s multirate
xsim multirate -runall > multirate.log 2>&1
cat multirate.log
rg -q 'Multirate controller checks passed' multirate.log
if rg -q 'Fatal:|FATAL:|ERROR:' multirate.log; then exit 1; fi
