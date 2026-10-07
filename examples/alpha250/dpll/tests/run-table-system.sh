#!/usr/bin/env bash
set -euo pipefail
if [[ -n ${DPLL_VIVADO_SETTINGS:-} ]]; then
    set +u
    source "$DPLL_VIVADO_SETTINGS"
    set -u
fi
here=$(cd "$(dirname "$0")" && pwd)
repo=$(cd "$here/../../../.." && pwd)
out=${DPLL_TABLE_SYSTEM_OUT:-"$repo/tmp/tests/alpha250-dpll/table-system"}
mkdir -p "$out"
cd "$out"
xvlog --sv "$XILINX_VIVADO/data/ip/xpm/xpm_cdc/hdl/xpm_cdc.sv" "$XILINX_VIVADO/data/verilog/src/glbl.v" "$repo/fpga/cores/axi_ctl_register_v1_0/axi_ctl_register.v" \
    "$repo/fpga/cores/axi_sts_register_v1_0/axi_sts_register.v" \
    "$here/../gain_programmer.v" "$here/../table_gain.v" "$here/../table_corrector.v" \
    "$here/test_table_system_tb.v" > compile.log 2>&1
xelab -L unisims_ver work.test_table_system_tb work.glbl -s test_table_system_tb > elaborate.log 2>&1
xsim test_table_system_tb -runall > simulation.log 2>&1
rg 'checks passed|Fatal:|ERROR:' simulation.log || true
rg -q 'Table system checks passed' simulation.log
if rg -q 'Fatal:|ERROR:|FATAL:' simulation.log; then exit 1; fi
