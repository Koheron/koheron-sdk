#!/usr/bin/env bash
set -euo pipefail
if [[ -n ${DPLL_VIVADO_SETTINGS:-} ]]; then
    set +u
    source "$DPLL_VIVADO_SETTINGS"
    set -u
fi
here=$(cd "$(dirname "$0")" && pwd)
repo=$(cd "$here/../../../.." && pwd)
out=${DPLL_PROGRAMMER_OUT:-"$repo/tmp/tests/alpha250-dpll/programmer"}
mkdir -p "$out"
cd "$out"
xvlog --sv "$XILINX_VIVADO/data/ip/xpm/xpm_cdc/hdl/xpm_cdc.sv" "$XILINX_VIVADO/data/verilog/src/glbl.v" "$here/../gain_programmer.v" "$here/test_gain_programmer_reset_tb.v" > compile.log 2>&1
xelab -L unisims_ver work.test_gain_programmer_reset_tb work.glbl -s test_gain_programmer_reset_tb > elaborate.log 2>&1
xsim test_gain_programmer_reset_tb -runall > simulation.log 2>&1
rg 'checks passed|Fatal:|ERROR:|FATAL:' simulation.log || true
rg -q 'Gain programmer reset checks passed' simulation.log
if rg -q 'Fatal:|ERROR:|FATAL:' simulation.log; then exit 1; fi
