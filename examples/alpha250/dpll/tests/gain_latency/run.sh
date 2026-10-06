#!/usr/bin/env bash
set -euo pipefail
if [[ -n ${DPLL_VIVADO_SETTINGS:-} ]]; then
    set +u
    source "$DPLL_VIVADO_SETTINGS"
    set -u
fi
here=$(cd "$(dirname "$0")" && pwd)
repo=$(cd "$here/../../../../.." && pwd)
out=${DPLL_GAIN_BENCH_OUT:-"$repo/tmp/tests/alpha250-dpll/gain-latency"}
mkdir -p "$out"
cd "$out"
python3 "$here/vectors.py" "$out/vectors.hex" > vectors.log
count=$(wc -l < vectors.hex)
xvlog --sv "$here/../../gain_multiplier.v" "$here/geometric_gain.v" "$here/test_tb.v" \
    "$XILINX_VIVADO/data/verilog/src/glbl.v" > compile.log 2>&1
xelab -L unisims_ver work.geometric_gain_test work.glbl -s geometric_gain_test > elaborate.log 2>&1
xsim geometric_gain_test -testplusarg "vectors=$out/vectors.hex" -testplusarg "count=$count" -runall > simulation.log 2>&1
cat vectors.log
rg 'Geometric gain checks passed|Production compatibility checks passed|Fatal:|ERROR:' simulation.log || true
rg -q 'Geometric gain checks passed' simulation.log
rg -q 'Production compatibility checks passed' simulation.log
if rg -q 'Fatal:|ERROR:|FATAL:' simulation.log; then exit 1; fi
if [[ ${DPLL_GAIN_SIM_ONLY:-0} == 1 ]]; then exit 0; fi
for mode in ${DPLL_GAIN_BENCH_MODES:-0 6 7}; do
    vivado -mode batch -nolog -nojournal -notrace -source "$here/benchmark.tcl" \
        -tclargs "$mode" "$out/mode-$mode" > "mode-$mode.log" 2>&1
    cat "$out/mode-$mode/result.txt"
done
