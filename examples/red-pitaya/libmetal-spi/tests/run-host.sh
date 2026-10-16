#!/usr/bin/env bash
set -euo pipefail
project=$(cd -- "$(dirname -- "$0")/.." && pwd)
docker run --rm -v "$project:/example:ro" -w /example \
    "${DOCKER_IMAGE:-koheron-libmetal-spi:2025.10}" bash -eu -c '
        g++-15 -std=c++23 -Wall -Wextra -Werror -pthread \
            -isystem /opt/libmetal-native/include -isystem /opt/xilinx-spi \
            -I. -Iport port.cpp tests/port.cpp \
            /opt/libmetal-native/lib/libmetal.a -lsysfs -o /tmp/test-port
        timeout 5 /tmp/test-port
    '
