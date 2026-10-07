#!/usr/bin/env bash
set -euo pipefail
if [[ -n ${DPLL_VIVADO_SETTINGS:-} ]]; then
    set +u
    source "$DPLL_VIVADO_SETTINGS"
    set -u
fi
repo=$(cd "$(dirname "$0")/../../../../.." && pwd)
out=${DPLL_REFERENCE_OUT:-"$repo/tmp/tests/alpha250-dpll/reference"}
cores=${DPLL_CORES:-"$repo/tmp/examples/alpha250/dpll/cores"}
mkdir -p "$out"
cd "$out"
vivado -mode batch -nolog -nojournal -notrace \
    -source "$repo/examples/alpha250/dpll/tests/reference/test_detector.tcl" \
    -tclargs "$cores" "$out/detector" > detector.log 2>&1
rg 'Detector checks passed|Fatal:|ERROR:' detector.log || true
rg -q 'Detector checks passed' detector.log
if rg -q 'Fatal:|ERROR:|FATAL:' detector.log; then exit 1; fi

xvlog --sv "$repo/examples/alpha250/dpll/tests/reference/gain_multiplier.v" \
    "$repo/examples/alpha250/dpll/tests/reference/test_gain_tb.v" \
    "$XILINX_VIVADO/data/verilog/src/glbl.v" > gain-compile.log 2>&1
xelab -L unisims_ver work.test_gain_tb work.glbl -s test_gain_tb > gain-elaborate.log 2>&1
xsim test_gain_tb -runall > gain.log 2>&1
rg 'Gain checks passed|Fatal:|ERROR:' gain.log || true
rg -q 'Gain checks passed' gain.log
if rg -q 'Fatal:|ERROR:|FATAL:' gain.log; then exit 1; fi

vivado -mode batch -nolog -nojournal -notrace \
    -source "$repo/examples/alpha250/dpll/tests/reference/test_corrector.tcl" \
    -tclargs "$cores" "$out/corrector" > corrector.log 2>&1
rg 'Corrector checks passed|Fatal:|ERROR:' corrector.log || true
rg -q 'Corrector checks passed' corrector.log
if rg -q 'Fatal:|ERROR:|FATAL:' corrector.log; then exit 1; fi
