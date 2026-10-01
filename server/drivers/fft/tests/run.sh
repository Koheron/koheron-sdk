#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../../../.."
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
for profile in '2048 14 125000000' '8192 16 250000000'; do
    read -r points bits rate <<< "$profile"
    g++ -std=c++20 -O2 -pthread -Wall -Wextra -Werror \
        -Iserver/drivers/fft/tests/stubs -I. -Iserver/external_libs -I/usr/include/eigen3 \
        -DTEST_FFT_SIZE="$points" -DTEST_ADC_WIDTH="$bits" -DTEST_ADC_RATE="$rate" \
        server/drivers/fft/tests/test_core.cpp -o "$work/test-core"
    timeout 10 "$work/test-core"
done
