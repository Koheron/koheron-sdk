#!/usr/bin/env bash
set -euo pipefail
if [[ -n ${DPLL_VIVADO_SETTINGS:-} ]]; then
    set +u
    source "$DPLL_VIVADO_SETTINGS"
    set -u
fi
here=$(cd "$(dirname "$0")" && pwd)
repo=$(cd "$here/../../../../.." && pwd)
out=${DPLL_TABLE_MEMORY_OUT:-"$repo/tmp/tests/alpha250-dpll/table-memory"}
mkdir -p "$out"
cd "$out"
xvlog --sv "$XILINX_VIVADO/data/verilog/src/glbl.v" "$here/../../table_gain.v" "$here/test_table_memory_tb.sv" > compile.log 2>&1
xelab -L unisims_ver work.test_table_memory_tb work.glbl -s test_table_memory_tb > elaborate.log 2>&1
xsim test_table_memory_tb -runall > simulation.log 2>&1
rg 'checks passed|Fatal:|ERROR:|FATAL:' simulation.log || true
[[ $(rg -c 'Table memory checks passed' simulation.log) == 8 ]]
if rg -q 'Fatal:|ERROR:|FATAL:' simulation.log; then exit 1; fi
