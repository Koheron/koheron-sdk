#!/usr/bin/env bash
set -euo pipefail
if [[ -n ${VIVADO_SETTINGS:-} ]]; then
    set +u
    source "$VIVADO_SETTINGS"
    set -u
fi
repo=$(cd "$(dirname "$0")/../../.." && pwd)
out="$repo/tmp/tests/axi-sts-register"
mkdir -p "$out"
cd "$out"
xvlog --sv "$repo/fpga/cores/axi_sts_register_v1_0/axi_sts_register.v" \
    "$repo/fpga/cores/axi_sts_register_v1_0/axi_sts_register_tb.sv" > compile.log 2>&1
for config in '1 32 16' '2 32 16' '3 32 16' '6 32 16' '11 32 16' \
              '20 32 16' '27 32 16' '32 32 16' '33 32 16' '65 32 16' \
              '11 64 16' '11 32 32'; do
    read -r words data_width addr_width <<< "$config"
    name="sts_${words}_${data_width}_${addr_width}"
    xelab work.axi_sts_register_tb -generic_top WORDS="$words" \
        -generic_top DATA_WIDTH="$data_width" -generic_top ADDR_WIDTH="$addr_width" \
        -s "$name" > "$name-elaborate.log" 2>&1
    xsim "$name" -runall > "$name.log" 2>&1
    rg 'PASS:|Fatal:|ERROR:' "$name.log" || true
    rg -q 'PASS: AXI status' "$name.log"
    if rg -q 'Fatal:|FATAL:|ERROR:' "$name.log"; then exit 1; fi
done
