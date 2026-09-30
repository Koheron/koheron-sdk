#!/usr/bin/env bash
set -euo pipefail
cd "$(git rev-parse --show-toplevel)"
pna_root="$PWD"
pna_tests=examples/alpha250-4/phase-noise-analyzer/tests
pna_output=tmp/tests/alpha250-4-phase-noise-analyzer
mkdir -p "$pna_output"
"${PNA_PYTHON:-.venv/bin/python3}" "$pna_tests/test_client.py"
docker run --rm -u "$(id -u):$(id -g)" -v "$pna_root:/review" -w /review \
    "${PNA_CPP_IMAGE:-cross-armhf:24.04}" bash -lc '
    set -euo pipefail
    tests=examples/alpha250-4/phase-noise-analyzer/tests
    output=tmp/tests/alpha250-4-phase-noise-analyzer
    flags=(-std=c++20 -g -pthread -fsanitize=address,undefined -I. -Iserver/external_libs)
    g++-13 "${flags[@]}" "$tests/test_core.cpp" -o "$output/core"
    "$output/core" "$output/measurements.bin" "$output/tracking.bin"
    g++-13 -I"$tests/stubs" "${flags[@]}" "$tests/test_dma.cpp" -o "$output/dma"
    "$output/dma"
    g++-13 -I"$tests/stubs" "${flags[@]}" "$tests/test_dma_api.cpp" -o "$output/dma_api"
    "$output/dma_api"
    g++-13 -I"$tests/stubs" "${flags[@]}" "$tests/test_dds.cpp" \
        examples/alpha250-4/phase-noise-analyzer/dds.cpp -o "$output/dds"
    "$output/dds"
'
docker run --rm -u "$(id -u):$(id -g)" -v "$pna_root:/review" -w /review \
    "${PNA_WEB_IMAGE:-koheron-web:node20}" node "$pna_tests/test_web.cjs" \
    tmp/examples/alpha250-4/phase-noise-analyzer/web/app.js \
    "$pna_output/measurements.bin" "$pna_output/tracking.bin"
