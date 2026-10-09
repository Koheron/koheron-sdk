#!/usr/bin/env bash
set -euo pipefail
cd "$(git rev-parse --show-toplevel)"
mkdir -p tmp/tests/alpha250-phase-noise-analyzer
docker run --rm -u "$(id -u):$(id -g)" -v "$PWD:/review" -w /review \
    "${PNA_ARM_IMAGE:-cross-armhf:24.04}" bash -lc '
    set -euo pipefail
    command -v qemu-arm >/dev/null
    output=tmp/tests/alpha250-phase-noise-analyzer/welch-arm
    # Keep the independent reference scalar; explicit NEON intrinsics still run.
    arm-linux-gnueabihf-g++-13 -std=c++23 -O3 -fno-tree-vectorize -mfpu=neon \
        -Wall -Wextra -Werror -pthread -I. -Iserver/external_libs \
        examples/alpha250/phase-noise-analyzer/tests/test_welch.cpp \
        server/external_libs/pffft/pffft.cpp -o "$output"
    qemu-arm -L /usr/arm-linux-gnueabihf "$output"
    '
