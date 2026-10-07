#!/usr/bin/env bash
set -euo pipefail
if [[ -n ${DPLL_VIVADO_SETTINGS:-} ]]; then
    set +u
    source "$DPLL_VIVADO_SETTINGS"
    set -u
fi
here=$(cd "$(dirname "$0")" && pwd)
repo=$(cd "$here/../../../../.." && pwd)
out=${DPLL_PHASE_TEST_OUT:-"$repo/tmp/tests/alpha250-dpll/phase-extraction"}
mkdir -p "$out"
python3 "$here/vectors.py" "$out/vectors.txt"
cd "$out"
xvlog --sv "$here/../../phase_residual.v" "$here/../../phase_extractor.v" "$here/test_tb.v" "$here/cordic_tb.v" "$here/legacy_tb.v" "$XILINX_VIVADO/data/verilog/src/glbl.v" > compile.log 2>&1
xelab -L unisims_ver work.phase_extraction_test work.glbl -s phase_extraction_test > elaborate.log 2>&1
xsim phase_extraction_test -testplusarg "VECTORS=$out/vectors.txt" -runall > simulation.log 2>&1
rg 'checks passed|Continuous atan2 error:|Before output rounding:|Fatal:|ERROR:|FATAL:' simulation.log || true
rg -q 'Phase extraction checks passed' simulation.log
if rg -q 'Fatal:|ERROR:|FATAL:' simulation.log; then exit 1; fi
xelab work.phase_extraction_cordic_test -s phase_extraction_cordic_test > cordic-elaborate.log 2>&1
xsim phase_extraction_cordic_test -testplusarg "VECTORS=$out/vectors.txt" -runall > cordic-simulation.log 2>&1
rg 'checks passed|Continuous atan2 error:|Before output rounding:|Fatal:|ERROR:|FATAL:' cordic-simulation.log || true
rg -q 'Phase extraction checks passed' cordic-simulation.log
if rg -q 'Fatal:|ERROR:|FATAL:' cordic-simulation.log; then exit 1; fi
xelab work.phase_extraction_legacy_test -s phase_extraction_legacy_test > legacy-elaborate.log 2>&1
xsim phase_extraction_legacy_test -testplusarg "VECTORS=$out/vectors.txt" -runall > legacy-simulation.log 2>&1
rg 'checks passed|Continuous atan2 error:|Before output rounding:|Fatal:|ERROR:|FATAL:' legacy-simulation.log || true
rg -q 'Phase extraction checks passed' legacy-simulation.log
if rg -q 'Fatal:|ERROR:|FATAL:' legacy-simulation.log; then exit 1; fi
xvlog --sv "$repo/fpga/cores/boxcar_filter_v1_0/boxcar_filter.v" \
    "$repo/fpga/cores/boxcar_filter_v1_0/boxcar_filter_tb.v" "$here/boxcar_tb.v" > boxcar-compile.log 2>&1
xelab work.phase_extraction_boxcar_test -s phase_extraction_boxcar_test > boxcar-elaborate.log 2>&1
xsim phase_extraction_boxcar_test -runall > boxcar.log 2>&1
rg 'checks passed|Fatal:|ERROR:|FATAL:' boxcar.log || true
rg -q '24-bit boxcar checks passed' boxcar.log
if rg -q 'Fatal:|ERROR:|FATAL:' boxcar.log; then exit 1; fi
if [[ ${DPLL_PHASE_ROUTE:-0} == 1 ]]; then
    vivado -mode batch -nolog -nojournal -notrace -source "$here/benchmark.tcl" \
        -tclargs 2 "$out/route" > route.log 2>&1
    cat route/result.txt
fi

if [[ ${DPLL_DETECTOR_ROUTE:-0} == 1 ]]; then
    vivado -mode batch -nolog -nojournal -notrace -source "$here/benchmark_detector.tcl" \
        -tclargs 2 "$out/detector-route" > detector-route.log 2>&1
    cat detector-route/result.txt
fi
