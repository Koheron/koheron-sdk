#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../../.."
project=examples/alpha250/vector-network-analyzer
output=tmp/$project
make_args=(CFG="$project/config.mk")
if [[ -n ${VIVADO_VERSION:-} ]]; then make_args+=(VIVADO_VERSION="$VIVADO_VERSION"); fi
make "${make_args[@]}" web
docker run --rm -u "$(id -u):$(id -g)" -v "$PWD:/work" -w /work \
    koheron-vna-wasm:llvm18 clang++-18 -std=c++23 -Wall -Wextra -Werror \
    "server/drivers/vector-network-analyzer/tests/test_plan.cpp" -o "$output/test-plan"
"$output/test-plan" > "$output/test-plan.json"
"$output/test-plan" 500 110000000 .1 -6 250000000 > "$output/test-plan-short.json"
"$output/test-plan" 1000000 5500000 .02 -6 250000000 > "$output/test-plan-narrow.json"
docker run --rm -v "$PWD:/work" -w /work koheron-web:node24 \
    node --test "web/vector-network-analyzer/tests/test_model.cjs" "web/vector-network-analyzer/tests/test_processor.cjs" \
    "web/tests/test_websocket_pool.cjs"
if [[ ${1:-} == --red-pitaya ]]; then
    make "${make_args[@]}" CFG=examples/red-pitaya/vector-network-analyzer/config.mk web
    red_output=tmp/examples/red-pitaya/vector-network-analyzer
    "$output/test-plan" 500 55000000 .25 -6 125000000 > "$red_output/test-plan.json"
    "$output/test-plan" 500 55000000 .1 -6 125000000 > "$red_output/test-plan-short.json"
    "$output/test-plan" 1000000 5500000 .02 -6 125000000 > "$red_output/test-plan-narrow.json"
    docker run --rm -v "$PWD:/work" -w /work koheron-web:node24 \
        node --test "web/vector-network-analyzer/tests/test_red_pitaya.cjs"
fi
