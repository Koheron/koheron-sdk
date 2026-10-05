#!/usr/bin/env bash
set -euo pipefail
if [[ -n "${PNA_VIVADO_SETTINGS:-}" ]]; then
    set +u
    source "$PNA_VIVADO_SETTINGS"
    set -u
fi
repo=$(cd "$(dirname "$0")/../../../.." && pwd)
out="$repo/tmp/tests/red-pitaya-phase-noise-analyzer/rtl"
mkdir -p "$out"
cd "$out"
shared="$repo/examples/alpha250-4/phase-noise-analyzer/tests"
xvlog "$repo/fpga/cores/axis_lfsr_v1_0/axis_lfsr.v" \
      "$repo/fpga/cores/phase_prefilter_v1_0/phase_prefilter.v"
xvlog --sv "$shared/test_phase_prefilter.sv"
for width in 16 24; do
    xelab work.test_phase_prefilter -generic_top WIDTH=$width -s prefilter_$width
    xsim prefilter_$width -runall > prefilter_$width.log 2>&1
    cat prefilter_$width.log
    rg -q 'Prefilter checks passed' prefilter_$width.log
    if rg -q 'Fatal:|ERROR:|FATAL:' prefilter_$width.log; then exit 1; fi
done
