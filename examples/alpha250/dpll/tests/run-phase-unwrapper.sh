#!/usr/bin/env bash
set -euo pipefail
if [[ -n ${DPLL_VIVADO_SETTINGS:-} ]]; then
    set +u
    source "$DPLL_VIVADO_SETTINGS"
    set -u
fi
here=$(cd "$(dirname "$0")" && pwd)
repo=$(cd "$here/../../../.." && pwd)
out=${DPLL_UNWRAPPER_OUT:-"$repo/tmp/tests/alpha250-dpll/unwrapper"}
mkdir -p "$out"
cd "$out"
xvlog --sv "$repo/fpga/cores/phase_unwrapper_v1_0/phase_unwrapper.v" "$here/test_phase_unwrapper_tb.v" > compile.log 2>&1
xelab work.test_phase_unwrapper_tb -s test_phase_unwrapper_tb > elaborate.log 2>&1
xsim test_phase_unwrapper_tb -runall > simulation.log 2>&1
rg 'checks passed|Fatal:|ERROR:|FATAL:' simulation.log || true
[[ $(rg -c 'Unwrapper checks passed' simulation.log) == 3 ]]
if rg -q 'Fatal:|ERROR:|FATAL:' simulation.log; then exit 1; fi
