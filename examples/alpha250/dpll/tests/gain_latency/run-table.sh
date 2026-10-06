#!/usr/bin/env bash
set -euo pipefail
if [[ -n ${DPLL_VIVADO_SETTINGS:-} ]]; then
    set +u
    source "$DPLL_VIVADO_SETTINGS"
    set -u
fi
here=$(cd "$(dirname "$0")" && pwd)
repo=$(cd "$here/../../../../.." && pwd)
out=${DPLL_GAIN_BENCH_OUT:-"$repo/tmp/tests/alpha250-dpll/table-gain"}
bits=${DPLL_GAIN_CHUNK_BITS:-4}
frac=${DPLL_GAIN_FRACTION_BITS:-11}
stages=${DPLL_GAIN_PIPE_STAGES:-3}
final_levels=${DPLL_GAIN_FINAL_CSA_LEVELS:-0}
carry_block=${DPLL_GAIN_CARRY_BLOCK:-0}
mkdir -p "$out"
cd "$out"
python3 "$here/table_vectors.py" "$out/vectors.hex" --chunk-bits "$bits" --fraction-bits "$frac" > vectors.log
count=$(wc -l < vectors.hex)
xvlog --sv "$here/../../table_gain.v" "$here/table_tb.v" > compile.log 2>&1
xelab work.table_gain_test -generic_top "CHUNK_BITS=$bits" -generic_top "FRACTION_BITS=$frac" \
    -generic_top "PIPE_STAGES=$stages" -generic_top "FINAL_CSA_LEVELS=$final_levels" -generic_top "CARRY_BLOCK=$carry_block" \
    -s table_gain_test > elaborate.log 2>&1
xsim table_gain_test -testplusarg "vectors=$out/vectors.hex" -testplusarg "count=$count" -runall > simulation.log 2>&1
cat vectors.log
rg 'Table gain checks passed|Fatal:|ERROR:' simulation.log || true
rg -q 'Table gain checks passed' simulation.log
if rg -q 'Fatal:|ERROR:|FATAL:' simulation.log; then exit 1; fi
