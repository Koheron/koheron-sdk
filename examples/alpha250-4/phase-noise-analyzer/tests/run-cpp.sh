#!/usr/bin/env bash
set -euo pipefail
repo=$(cd "$(dirname "$0")/../../../.." && pwd)
source "$repo/server/drivers/phase-noise/tests/host-test-env.sh"
tests=examples/alpha250-4/phase-noise-analyzer/tests
output=tmp/tests/alpha250-4-phase-noise-analyzer
mkdir -p "$output"

"$PNA_TEST_CXX" "${flags[@]}" "$tests/test_core.cpp" -o "$output/core"
"$output/core" "$output/measurements.bin" "$output/tracking.bin"
"$PNA_TEST_CXX" "${flags[@]}" "$tests/test_config_settings.cpp" -o "$output/config_settings"
"$output/config_settings"
"$PNA_TEST_CXX" "${flags[@]}" "$tests/test_phase_spectrum.cpp" server/external_libs/pffft/pffft.cpp -o "$output/phase_spectrum"
"$output/phase_spectrum"
"$PNA_TEST_CXX" "${flags[@]}" "$tests/test_single_window_spectrum.cpp" server/external_libs/pffft/pffft.cpp -o "$output/single_window_spectrum"
"$output/single_window_spectrum"
"$PNA_TEST_CXX" -std=c++23 -O2 -pthread -I. -Iserver/external_libs -I/usr/include/eigen3 \
    "$tests/check_cross_density.cpp" server/external_libs/pffft/pffft.cpp -o "$output/check_cross_density"
"$PNA_TEST_CXX" "${flags[@]}" "$tests/test_decimation.cpp" -o "$output/decimation"
"$output/decimation"
"$PNA_TEST_CXX" -I"$tests/stubs" "${flags[@]}" "$tests/test_dma.cpp" -o "$output/dma"
"$output/dma"
"$PNA_TEST_CXX" -I"$tests/stubs" "${flags[@]}" "$tests/test_dma_api.cpp" -o "$output/dma_api"
"$output/dma_api"
"$PNA_TEST_CXX" -I"$tests/stubs" "${flags[@]}" "$tests/test_dds.cpp" \
    examples/alpha250-4/phase-noise-analyzer/dds.cpp -o "$output/dds"
"$output/dds"
"$PNA_TEST_CXX" -I"$tests/stubs" \
    -Iexamples/alpha250/phase-noise-analyzer/tests/stubs "${flags[@]}" \
    "$tests/test_settings.cpp" \
    examples/alpha250-4/phase-noise-analyzer/phase-noise-analyzer.cpp \
    examples/alpha250-4/phase-noise-analyzer/dds.cpp \
    server/external_libs/pffft/pffft.cpp -o "$output/settings"
"$output/settings"
"$output/settings" --saved
