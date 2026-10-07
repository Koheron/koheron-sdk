#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/host-test-env.sh"
output=tmp/tests/phase-noise
mkdir -p "$output"
"$PNA_TEST_CXX" "${flags[@]}" -Wall -Wextra -Werror \
    server/drivers/phase-noise/tests/test_spectrum_publication.cpp -o "$output/publication"
"$output/publication" "$output/spectrum-frame.bin"
"$PNA_TEST_CXX" -Iserver/drivers/phase-noise/tests/stubs "${flags[@]}" \
    server/drivers/phase-noise/tests/test_cyclic_phase_dma.cpp -o "$output/cyclic-dma"
"$output/cyclic-dma"
"$PNA_TEST_CXX" "${flags[@]}" server/drivers/phase-noise/tests/test_streaming_welch.cpp \
    server/external_libs/pffft/pffft.cpp -o "$output/streaming-welch"
"$output/streaming-welch" "$output/streaming-welch.bin"
