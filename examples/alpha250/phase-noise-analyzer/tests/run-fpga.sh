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
