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
xvlog --sv "$repo/fpga/cores/axi_ctl_register_v1_0/axi_ctl_register.v" \
    "$repo/fpga/cores/axi_sts_register_v1_0/axi_sts_register.v" \
    "$here/../gain_programmer.v" "$here/../table_gain.v" "$here/../table_corrector.v" \
    "$here/test_table_system_tb.v" > compile.log 2>&1
xelab work.test_table_system_tb -s test_table_system_tb > elaborate.log 2>&1
xsim test_table_system_tb -runall > simulation.log 2>&1
rg 'checks passed|Fatal:|ERROR:' simulation.log || true
rg -q 'Table system checks passed' simulation.log
if rg -q 'Fatal:|ERROR:|FATAL:' simulation.log; then exit 1; fi

# Check the historical interface as well as the selected eight extra phase bits.
xelab work.test_table_system_tb -generic_top PHASE_FRACTION_BITS=0 -s test_table_system_legacy > legacy-elaborate.log 2>&1
xsim test_table_system_legacy -runall > legacy-simulation.log 2>&1
rg 'checks passed|Fatal:|ERROR:|FATAL:' legacy-simulation.log || true
rg -q 'Table system checks passed' legacy-simulation.log
if rg -q 'Fatal:|ERROR:|FATAL:' legacy-simulation.log; then exit 1; fi
