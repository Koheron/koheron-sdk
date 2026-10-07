#!/usr/bin/env bash
set -euo pipefail
if [[ -n ${DPLL_VIVADO_SETTINGS:-} ]]; then
    set +u
    source "$DPLL_VIVADO_SETTINGS"
    set -u
fi
here=$(cd "$(dirname "$0")" && pwd)
repo=$(cd "$here/../../../.." && pwd)
out=${DPLL_DAC_MUX_OUT:-"$repo/tmp/tests/alpha250-dpll/dac-mux"}
mkdir -p "$out"
cd "$out"
xvlog --sv "$repo/fpga/cores/latched_mux_v1_0/latched_mux.v" "$repo/fpga/cores/latched_mux_v1_0/latched_mux_tb.v" > compile.log 2>&1
for stages in 1 2; do
    xelab work.latched_mux_tb -generic_top "OUTPUT_STAGES=$stages" -generic_top "WIDTH=16" -generic_top "N_INPUTS=8" -generic_top "SEL_WIDTH=3" -s "dac_mux_$stages" > "elaborate-$stages.log" 2>&1
    xsim "dac_mux_$stages" -runall > "simulation-$stages.log" 2>&1
    rg 'checks passed|Fatal:|ERROR:' "simulation-$stages.log" || true
    rg -q 'Latched mux checks passed' "simulation-$stages.log"
    if rg -q 'Fatal:|ERROR:|FATAL:' "simulation-$stages.log"; then exit 1; fi
done
