#!/usr/bin/env bash
set -euo pipefail
if [[ -n ${DPLL_VIVADO_SETTINGS:-} ]]; then
    set +u
    source "$DPLL_VIVADO_SETTINGS"
    set -u
fi
here=$(cd "$(dirname "$0")" && pwd)
repo=$(cd "$here/../../../../.." && pwd)
out=${DPLL_PNA_TEST_OUT:-"$repo/tmp/tests/alpha250-dpll/pna-comparison"}
core=${DPLL_PNA_CORE:-"$repo/tmp/examples/alpha250-4/phase-noise-analyzer/fpga/phase-noise-analyzer.gen/sources_1/bd/system/ip/system_cordic_0/sim/system_cordic_0.vhd"}
if [[ ! -f $core ]]; then
    echo "Set DPLL_PNA_CORE to the generated PNA system_cordic_0 simulation VHDL" >&2
    exit 1
fi
# Check the actual generated core, rather than substituting a software model.
for field in 'C_INPUT_WIDTH => 24' 'C_OUTPUT_WIDTH => 24' 'C_PHASE_FORMAT => 1' 'C_PIPELINE_MODE => -2' 'C_ROUND_MODE => 2'; do
    rg -Fq "$field" "$core"
done
mkdir -p "$out"
python3 "$here/vectors.py" "$out/native-vectors.txt"
python3 "$here/pna_compare.py" vectors "$out/native-vectors.txt" "$out/vectors.txt"
cd "$out"
xvhdl --work xil_defaultlib "$core" > vendor-compile.log 2>&1
xvlog --sv "$here/../../phase_extractor.v" "$here/../../phase_residual.v" \
    "$here/pna_tb.v" "$XILINX_VIVADO/data/verilog/src/glbl.v" > compile.log 2>&1
xelab -L xil_defaultlib -L cordic_v6_0_24 -L unisims_ver work.pna_phase_compare \
    work.glbl -s pna_compare > elaborate.log 2>&1
xsim pna_compare -runall > simulation.log 2>&1
rg 'Direct comparison passed|Fatal:|ERROR:|FATAL:' simulation.log || true
rg -q 'Direct comparison passed' simulation.log
if rg -q 'Fatal:|ERROR:|FATAL:' simulation.log; then exit 1; fi
python3 "$here/pna_compare.py" report "$out/results.txt" "$out/comparison.json"
