#!/usr/bin/env bash
set -euo pipefail

if [[ -n ${DPLL_VIVADO_SETTINGS:-} ]]; then
    set +u
    source "$DPLL_VIVADO_SETTINGS"
    set -u
fi
repo=$(cd "$(dirname "$0")/../../../.." && pwd)
out=${DPLL_TEST_OUT:-"$repo/tmp/tests/alpha250-dpll"}
cores=${DPLL_CORES:-"$repo/tmp/examples/alpha250/dpll/cores"}
mkdir -p "$out"
cd "$out"

# Package the instrument cores with make xpr before running this test.
xvlog --sv "$repo/fpga/cores/boxcar_filter_v1_0/boxcar_filter.v" \
    "$repo/fpga/cores/boxcar_filter_v1_0/boxcar_filter_tb.v" > boxcar-compile.log 2>&1
xelab work.boxcar_filter_tb -s boxcar_filter_tb > boxcar-elaborate.log 2>&1
xsim boxcar_filter_tb -runall > boxcar.log 2>&1
cat boxcar.log
rg -q 'Boxcar checks passed' boxcar.log
if rg -q 'Fatal:|ERROR:|FATAL:' boxcar.log; then exit 1; fi

vivado -mode batch -nolog -nojournal -notrace \
    -source "$repo/examples/alpha250/dpll/tests/test_detector.tcl" \
    -tclargs "$cores" "$out/detector" > detector.log 2>&1
rg 'Detector checks passed|Fatal:|ERROR:' detector.log || true
rg -q 'Detector checks passed' detector.log
if rg -q 'Fatal:|ERROR:|FATAL:' detector.log; then exit 1; fi
