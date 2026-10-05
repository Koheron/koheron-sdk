#!/usr/bin/env bash
set -euo pipefail
cd "$(git rev-parse --show-toplevel)"
pna_root="$PWD"
pna_tests=examples/alpha250/phase-noise-analyzer/tests
pna_output=tmp/tests/alpha250-phase-noise-analyzer
mkdir -p "$pna_output"
"${PNA_PYTHON:-.venv/bin/python3}" "$pna_tests/test_client.py"
"${PNA_PYTHON:-.venv/bin/python3}" "$pna_tests/test_phase_calibration.py"
"${PNA_PYTHON:-.venv/bin/python3}" "$pna_tests/test_phase_modulator.py"
docker run --rm -u "$(id -u):$(id -g)" -v "$pna_root:/review" -w /review \
    "${PNA_CPP_IMAGE:-cross-armhf:24.04}" bash -lc '
    set -euo pipefail
    tests=examples/alpha250/phase-noise-analyzer/tests
    output=tmp/tests/alpha250-phase-noise-analyzer
    flags=(-std=c++20 -g -Wall -Wextra -Werror -pthread -fsanitize=address,undefined -I. -Iserver/external_libs)
    g++-13 -I"$tests/stubs" "${flags[@]}" "$tests/test_acquisition.cpp" \
        examples/alpha250/phase-noise-analyzer/phase-noise-analyzer.cpp \
        examples/alpha250/phase-noise-analyzer/dds.cpp \
        server/external_libs/pffft/pffft.cpp -o "$output/acquisition"
    "$output/acquisition"
    g++-13 -I"$tests/stubs" "${flags[@]}" "$tests/test_tracking.cpp" \
        examples/alpha250/phase-noise-analyzer/phase-noise-analyzer.cpp \
        examples/alpha250/phase-noise-analyzer/dds.cpp \
        server/external_libs/pffft/pffft.cpp -o "$output/tracking"
    "$output/tracking"
    g++-13 -I"$tests/stubs" "${flags[@]}" "$tests/test_dds.cpp" \
        examples/alpha250/phase-noise-analyzer/dds.cpp -o "$output/dds"
    "$output/dds"
    g++-13 "${flags[@]}" "$tests/test_moving_averager.cpp" -o "$output/averager"
    "$output/averager"
    for backend in simd scalar; do
        extra=()
        if [ "$backend" = scalar ]; then extra=(-DPFFFT_SIMD_DISABLE); fi
        g++-13 "${flags[@]}" "${extra[@]}" "$tests/test_welch.cpp" \
            server/external_libs/pffft/pffft.cpp -o "$output/welch-$backend"
        "$output/welch-$backend"
    done
    g++-13 "${flags[@]}" server/drivers/phase-noise/tests/test_spectrum_publication.cpp -o "$output/publication"
    "$output/publication" "$output/spectrum-frame.bin"
    g++-13 -Iserver/drivers/phase-noise/tests/stubs "${flags[@]}" server/drivers/phase-noise/tests/test_cyclic_phase_dma.cpp -o "$output/cyclic-dma"
    "$output/cyclic-dma"
    # Both instruments use the extracted phase conversion/detrending helper.
    g++-13 "${flags[@]}" examples/alpha250-4/phase-noise-analyzer/tests/test_core.cpp -o "$output/alpha250-4-core"
    "$output/alpha250-4-core"
    g++-13 "${flags[@]}" examples/alpha250-4/phase-noise-analyzer/tests/test_phase_spectrum.cpp server/external_libs/pffft/pffft.cpp -o "$output/alpha250-4-spectrum"
    "$output/alpha250-4-spectrum"
'
docker run --rm -u "$(id -u):$(id -g)" -v "$pna_root:/review" -w /review \
    "${PNA_WEB_IMAGE:-koheron-web:node20}" sh -c '
    set -eu
    tests=examples/alpha250/phase-noise-analyzer/tests
    deps=tmp/tests/alpha250-phase-noise-analyzer/web-deps
    export NODE_PATH="$PWD/$deps/node_modules:/opt/app/node_modules"
    if ! node -e "require.resolve(\"jsdom\"); require.resolve(\"typescript\")" >/dev/null 2>&1; then
        npm install --prefix "$deps" --no-save --package-lock=false typescript@5.6.3 jsdom@26.1.0
    fi
    node --test "$tests/test_plot.cjs" "$tests/test_rendering.cjs" "$tests/test_numeric_controls.cjs" "$tests/test_signal_generator.cjs" web/phase-noise/tests/test_phase_precision.cjs web/phase-noise/tests/test_spectrum.cjs
'
