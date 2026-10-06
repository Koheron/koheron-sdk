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

xvlog --sv "$repo/examples/alpha250/dpll/gain_multiplier.v" \
    "$repo/examples/alpha250/dpll/tests/test_gain_tb.v" \
    "$XILINX_VIVADO/data/verilog/src/glbl.v" > gain-compile.log 2>&1
xelab -L unisims_ver work.test_gain_tb work.glbl -s test_gain_tb > gain-elaborate.log 2>&1
xsim test_gain_tb -runall > gain.log 2>&1
rg 'Gain checks passed|Fatal:|ERROR:' gain.log || true
rg -q 'Gain checks passed' gain.log
if rg -q 'Fatal:|ERROR:|FATAL:' gain.log; then exit 1; fi

xvlog --sv "$repo/fpga/cores/axi_ctl_register_v1_0/axi_ctl_register.v" \
    "$repo/examples/alpha250/dpll/tests/test_control_tb.v" > control-compile.log 2>&1
xelab work.test_control_tb -s test_control_tb > control-elaborate.log 2>&1
xsim test_control_tb -runall > control.log 2>&1
rg 'Control checks passed|Fatal:|ERROR:' control.log || true
rg -q 'Control checks passed' control.log
if rg -q 'Fatal:|ERROR:|FATAL:' control.log; then exit 1; fi

vivado -mode batch -nolog -nojournal -notrace \
    -source "$repo/examples/alpha250/dpll/tests/test_corrector.tcl" \
    -tclargs "$cores" "$out/corrector" > corrector.log 2>&1
rg 'Corrector checks passed|Fatal:|ERROR:' corrector.log || true
rg -q 'Corrector checks passed' corrector.log
if rg -q 'Fatal:|ERROR:|FATAL:' corrector.log; then exit 1; fi

export DPLL_TEST_PYTHON=${DPLL_TEST_PYTHON:-"$repo/.venv/bin/python3"}
vivado -mode batch -nolog -nojournal -notrace \
    -source "$repo/examples/alpha250/dpll/tests/test_monitor.tcl" \
    -tclargs "$cores" "$out/monitor" > monitor.log 2>&1
rg 'Monitor checks passed|Fatal:|ERROR:' monitor.log || true
rg -q 'Monitor checks passed' monitor.log
if rg -q 'Fatal:|ERROR:|FATAL:' monitor.log; then exit 1; fi

vivado -mode batch -nolog -nojournal -notrace \
    -source "$repo/examples/alpha250/dpll/tests/test_cic.tcl" \
    -tclargs "$cores" "$out/cic" > cic.log 2>&1
rg 'CIC checks passed|Fatal:|ERROR:' cic.log || true
rg -q 'CIC checks passed' cic.log
if rg -q 'Fatal:|ERROR:|FATAL:' cic.log; then exit 1; fi

# Production table-gain arithmetic and the complete two-loop programming path.
bash "$repo/examples/alpha250/dpll/tests/gain_latency/run-corrector.sh"
bash "$repo/examples/alpha250/dpll/tests/run-table-system.sh"
bash "$repo/examples/alpha250/dpll/tests/phase_extraction/run.sh"

# Width/scaling validation for full-precision controller feedback only.
vivado -mode batch -nolog -nojournal -notrace \
    -source "$repo/examples/alpha250/dpll/tests/check_phase_feedback.tcl" \
    -tclargs "$cores" "$out/phase-feedback" > phase-feedback.log 2>&1
rg 'checks passed|Fatal:|ERROR:' phase-feedback.log || true
rg -q 'Phase feedback wiring checks passed' phase-feedback.log
if rg -q 'Fatal:|ERROR:|FATAL:' phase-feedback.log; then exit 1; fi
