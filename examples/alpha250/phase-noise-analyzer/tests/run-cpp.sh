#!/usr/bin/env bash
set -euo pipefail
repo=$(cd "$(dirname "$0")/../../../.." && pwd)
source "$repo/server/drivers/phase-noise/tests/host-test-env.sh"
tests=examples/alpha250/phase-noise-analyzer/tests
output=tmp/tests/alpha250-phase-noise-analyzer
mkdir -p "$output"
flags+=(-Wall -Wextra -Werror)
"$PNA_TEST_CXX" -I"$tests/stubs" "${flags[@]}" "$tests/test_acquisition.cpp" \
    examples/alpha250/phase-noise-analyzer/phase-noise-analyzer.cpp \
    examples/alpha250/phase-noise-analyzer/dds.cpp \
    server/external_libs/pffft/pffft.cpp -o "$output/acquisition"
"$output/acquisition"
"$PNA_TEST_CXX" -I"$tests/stubs" "${flags[@]}" "$tests/test_tracking.cpp" \
    examples/alpha250/phase-noise-analyzer/phase-noise-analyzer.cpp \
    examples/alpha250/phase-noise-analyzer/dds.cpp \
    server/external_libs/pffft/pffft.cpp -o "$output/tracking"
"$output/tracking"
"$PNA_TEST_CXX" -I"$tests/stubs" "${flags[@]}" "$tests/test_sample_rate.cpp" \
    examples/alpha250/phase-noise-analyzer/phase-noise-analyzer.cpp \
    examples/alpha250/phase-noise-analyzer/dds.cpp \
    server/external_libs/pffft/pffft.cpp -o "$output/sample-rate"
"$output/sample-rate"
"$output/sample-rate" --saved
"$PNA_TEST_CXX" -I"$tests/stubs" "${flags[@]}" "$tests/test_dds.cpp" \
    examples/alpha250/phase-noise-analyzer/dds.cpp -o "$output/dds"
"$output/dds"
"$PNA_TEST_CXX" "${flags[@]}" "$tests/test_moving_averager.cpp" -o "$output/averager"
"$output/averager"
for backend in simd scalar; do
    extra=()
    if [ "$backend" = scalar ]; then extra=(-DPFFFT_SIMD_DISABLE); fi
    "$PNA_TEST_CXX" "${flags[@]}" "${extra[@]}" "$tests/test_welch.cpp" \
        server/external_libs/pffft/pffft.cpp -o "$output/welch-$backend"
    "$output/welch-$backend"
    "$PNA_TEST_CXX" "${flags[@]}" "${extra[@]}" server/external_libs/pffft/tests/test_transform.cpp \
        server/external_libs/pffft/pffft.cpp -o "$output/fft-$backend"
    "$output/fft-$backend"
done
