#!/usr/bin/env bash
set -euo pipefail
if [[ -n ${DPLL_VIVADO_SETTINGS:-} ]]; then
    set +u
    source "$DPLL_VIVADO_SETTINGS"
    set -u
fi
here=$(cd "$(dirname "$0")" && pwd)
repo=$(cd "$here/../../../../.." && pwd)
out=${DPLL_GAIN_BENCH_OUT:-"$repo/tmp/tests/alpha250-dpll/constant-gain"}
mkdir -p "$out"
cd "$out"
vector_options=()
top=constant_gain_test
if [[ ${DPLL_GAIN_FRACTION_BITS:-11} == 7 ]]; then
    vector_options+=(--sparse)
    top=constant_gain_test_7
fi
python3 "$here/vectors.py" "$out/vectors.hex" "${vector_options[@]}" > vectors.log
count=$(wc -l < vectors.hex)
xvlog --sv "$here/constant_gain.v" "$here/constant_tb.v" > compile.log 2>&1
xelab "work.$top" -s constant_gain_test > elaborate.log 2>&1
xsim constant_gain_test -testplusarg "vectors=$out/vectors.hex" -testplusarg "count=$count" -runall > simulation.log 2>&1
cat vectors.log
rg 'Constant gain checks passed|Fatal:|ERROR:' simulation.log || true
rg -q 'Constant gain checks passed' simulation.log
if rg -q 'Fatal:|ERROR:|FATAL:' simulation.log; then exit 1; fi
if [[ ${DPLL_GAIN_SIM_ONLY:-0} == 1 ]]; then exit 0; fi
for mode in ${DPLL_GAIN_BENCH_MODES:-8 9}; do
    vivado -mode batch -nolog -nojournal -notrace -source "$here/benchmark.tcl" \
        -tclargs "$mode" "$out/mode-$mode" > "mode-$mode.log" 2>&1
    cat "$out/mode-$mode/result.txt"
done
