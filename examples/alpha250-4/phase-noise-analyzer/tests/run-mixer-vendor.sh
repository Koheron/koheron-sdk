#!/usr/bin/env bash
set -euo pipefail
if [[ -n "${PNA_VIVADO_SETTINGS:-}" ]]; then
    set +u
    source "$PNA_VIVADO_SETTINGS"
    set -u
fi
repo=$(cd "$(dirname "$0")/../../../.." && pwd)
model=${PNA_VENDOR_MIXER_MODEL:-$repo/tmp/examples/alpha250-4/phase-noise-analyzer/fpga/phase-noise-analyzer.gen/sources_1/bd/system/ip/system_complex_mult_0/sim/system_complex_mult_0.vhd}
if [[ ! -f "$model" ]]; then
    echo "Missing generated full-product CMPY model: build the FPGA project first." >&2
    exit 1
fi
out="$repo/tmp/tests/alpha250-4-phase-noise-analyzer/mixer-vendor"
mkdir -p "$out"
cd "$out"
xvhdl "$model"
xvlog "$repo/examples/alpha250-4/phase-noise-analyzer/phase_round_v1_0/phase_round.v"
xvlog --sv "$repo/examples/alpha250-4/phase-noise-analyzer/tests/test_mixer_product.sv"
xelab -L cmpy_v6_0_26 work.test_mixer_product -s mixer_product
xsim mixer_product -runall > simulation.log 2>&1
cat simulation.log
rg -q 'Vendor mixer checks passed' simulation.log
if rg -q 'Fatal:|ERROR:|FATAL:' simulation.log; then
    exit 1
fi
