#!/usr/bin/env bash
set -euo pipefail

if [[ -n "${PNA_VIVADO_SETTINGS:-}" ]]; then
    set +u
    source "$PNA_VIVADO_SETTINGS"
    set -u
fi
repo=$(cd "$(dirname "$0")/../../../.." && pwd)
out="$repo/tmp/tests/alpha250-phase-noise-analyzer/rtl"
mkdir -p "$out"
cd "$out"

# Reuse the DPLL's legacy/predecoded AXI equivalence and independent scoreboard
# at this analyzer's 12-word register-bank size. No additional control latency.
xvlog --sv "$repo/fpga/cores/axi_ctl_register_v1_0/axi_ctl_register.v" \
      "$repo/examples/alpha250/dpll/tests/test_control_tb.v" > control-compile.log 2>&1
xelab work.test_control_tb -generic_top WORDS=12 -s test_pna_control_tb > control-elaborate.log 2>&1
xsim test_pna_control_tb -runall > control.log 2>&1
cat control.log
rg -q 'Control checks passed' control.log
if rg -q 'Fatal:|ERROR:|FATAL:' control.log; then exit 1; fi

# The 250 MHz history splits the 64-bit sum without dropping ADC samples.
xvlog --sv "$repo/fpga/cores/phase_unwrapper_v1_0/phase_unwrapper.v" \
      "$repo/examples/alpha250/phase-noise-analyzer/tests/test_phase_history_tb.sv" \
      "$XILINX_VIVADO/data/verilog/src/glbl.v"
xelab -L unisims_ver work.test_pna_phase_history_tb work.glbl -s test_pna_phase_history_tb
xsim test_pna_phase_history_tb -runall > history.log 2>&1
cat history.log
rg -q 'PNA phase history checks passed' history.log
if rg -q 'Fatal:|ERROR:|FATAL:' history.log; then
    exit 1
fi

# Both instruments instantiate these same RTL cores with the same first two
# seeds. Reuse the independent convolution and mixer-response regressions.
shared_tests="$repo/examples/alpha250-4/phase-noise-analyzer/tests"
xvlog "$repo/fpga/cores/axis_lfsr_v1_0/axis_lfsr.v" \
      "$repo/fpga/cores/phase_prefilter_v1_0/phase_prefilter.v"
xvlog --sv "$shared_tests/test_phase_prefilter.sv"
xelab work.test_phase_prefilter -s test_phase_prefilter
xsim test_phase_prefilter -runall > prefilter.log 2>&1
cat prefilter.log
rg -q 'Prefilter checks passed' prefilter.log
if rg -q 'Fatal:|ERROR:|FATAL:' prefilter.log; then
    exit 1
fi
"${PNA_PYTHON:-$repo/.venv/bin/python3}" "$shared_tests/test_prefilter_response.py"
