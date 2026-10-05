#!/usr/bin/env bash
set -euo pipefail

# Optionally source Vivado settings; otherwise use the tools already in PATH.
if [[ -n "${PNA_VIVADO_SETTINGS:-}" ]]; then
    set +u
    source "$PNA_VIVADO_SETTINGS"
    set -u
fi
repo=$(cd "$(dirname "$0")/../../../.." && pwd)
out="$repo/tmp/tests/alpha250-4-phase-noise-analyzer/rtl"
mkdir -p "$out"
cd "$out"

xvlog "$repo/fpga/cores/phase_unwrapper_v1_0/phase_unwrapper.v" \
      "$repo/fpga/cores/boxcar_filter_v1_0/boxcar_filter.v" \
      "$repo/fpga/cores/axis_lfsr_v1_0/axis_lfsr.v" \
      "$repo/fpga/cores/phase_prefilter_v1_0/phase_prefilter.v"
xvlog "$repo/fpga/cores/phase_range_guard_v1_0/phase_range_guard.v"
xvlog "$repo/fpga/cores/phase_stochastic_round_v1_0/phase_stochastic_round.v"
xvlog --sv "$repo/examples/alpha250-4/phase-noise-analyzer/tests/test_phase_rounding.sv"
xelab work.test_phase_rounding -s test_phase_rounding
xsim test_phase_rounding -runall > phase-rounding.log 2>&1
cat phase-rounding.log
rg -q 'Phase rounding checks passed' phase-rounding.log
if rg -q 'Fatal:|ERROR:|FATAL:' phase-rounding.log; then exit 1; fi
xvlog --sv "$repo/examples/alpha250-4/phase-noise-analyzer/tests/test_phase_headroom.sv"
xelab work.test_phase_headroom -s test_phase_headroom
xsim test_phase_headroom -runall > headroom.log 2>&1
cat headroom.log
rg -q 'Phase headroom checks passed' headroom.log
if rg -q 'Fatal:|ERROR:|FATAL:' headroom.log; then exit 1; fi
xvlog "$repo/examples/alpha250-4/phase-noise-analyzer/paired_cic_control_v1_0/paired_cic_control.v"
xvlog --sv "$repo/examples/alpha250-4/phase-noise-analyzer/tests/test_paired_cic_control.sv"
xelab work.test_paired_cic_control -s paired_cic_control
xsim paired_cic_control -runall > paired-cic-control.log 2>&1
cat paired-cic-control.log
rg -q 'Paired CIC control checks passed' paired-cic-control.log
if rg -q 'Fatal:|ERROR:|FATAL:' paired-cic-control.log; then
    exit 1
fi
xvlog "$repo/fpga/cores/phase_stream_control_v1_0/phase_stream_control.v"
xvlog --sv "$repo/fpga/cores/phase_stream_control_v1_0/phase_stream_control_tb.sv"
xelab work.phase_stream_control_tb -s phase_stream_control_tb
xsim phase_stream_control_tb -runall > single-stream-control.log 2>&1
cat single-stream-control.log
rg -q 'Single-stream control checks passed' single-stream-control.log
if rg -q 'Fatal:|ERROR:|FATAL:' single-stream-control.log; then exit 1; fi
xvlog --sv "$repo/examples/alpha250-4/phase-noise-analyzer/tests/test_fpga_blocks.sv"
xelab work.test_fpga_blocks -s audit_fpga
xsim audit_fpga -runall > simulation.log 2>&1
cat simulation.log
rg -q 'FPGA block checks passed' simulation.log
if rg -q 'Fatal:|ERROR:|FATAL:' simulation.log; then
    exit 1
fi
xvlog --sv "$repo/examples/alpha250-4/phase-noise-analyzer/tests/test_phase_prefilter.sv"
xelab work.test_phase_prefilter -s test_phase_prefilter
xsim test_phase_prefilter -runall > prefilter.log 2>&1
cat prefilter.log
rg -q 'Prefilter checks passed' prefilter.log
if rg -q 'Fatal:|ERROR:|FATAL:' prefilter.log; then
    exit 1
fi
"${PNA_PYTHON:-$repo/.venv/bin/python3}" "$repo/examples/alpha250-4/phase-noise-analyzer/tests/test_prefilter_response.py"
xvlog "$repo/fpga/cores/phase_quantizer_v1_0/phase_quantizer.v" \
      "$repo/fpga/cores/axis_stream_packet_mux_v1_0/axis_stream_packet_mux.v"
xvlog --sv "$repo/fpga/cores/phase_quantizer_v1_0/phase_quantizer_tb.sv" \
      "$repo/examples/alpha250-4/phase-noise-analyzer/tests/test_packet_metadata.sv"
for pna_test in phase_quantizer_tb test_packet_metadata; do
    xelab "work.$pna_test" -s "$pna_test"
    xsim "$pna_test" -runall > "$pna_test.log" 2>&1
    cat "$pna_test.log"
    rg -q 'passed' "$pna_test.log"
    if rg -q 'Fatal:|ERROR:|FATAL:' "$pna_test.log"; then exit 1; fi
done
