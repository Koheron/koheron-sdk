#!/usr/bin/env bash
set -euo pipefail
cd "$(git rev-parse --show-toplevel)"
mkdir -p tmp/tests/alpha250-dpll
"${PNA_PYTHON:-.venv/bin/python3}" examples/alpha250/dpll/tests/test_monitor_client.py
docker run --rm -u "$(id -u):$(id -g)" -v "$PWD:/review" -w /review \
  "${PNA_CPP_IMAGE:-cross-armhf:24.04}" bash -lc '
  set -euo pipefail
  g++-13 -std=c++20 -Wall -Wextra -Werror -g -pthread -fsanitize=address,undefined \
    -Iexamples/alpha250/phase-noise-analyzer/tests/stubs -I. -Iserver/external_libs \
    examples/alpha250/dpll/tests/test_monitor_core.cpp server/external_libs/pffft/pffft.cpp \
    -o tmp/tests/alpha250-dpll/monitor-core
  tmp/tests/alpha250-dpll/monitor-core
  g++-13 -std=c++20 -Wall -Wextra -Werror -g -pthread -fsanitize=address,undefined \
    -Iexamples/alpha250/dpll/tests/dma-stubs \
    -Iexamples/alpha250/phase-noise-analyzer/tests/stubs -I. -Iserver/external_libs \
    examples/alpha250/dpll/tests/test_dma_guard.cpp -o tmp/tests/alpha250-dpll/dma-guard
  tmp/tests/alpha250-dpll/dma-guard
'
set +u
source "${DPLL_VIVADO_SETTINGS:-/tools/Xilinx/2025.1/Vivado/settings64.sh}"
set -u
vivado -mode batch -nolog -nojournal -notrace \
  -source examples/alpha250/dpll/tests/test_monitor_cdc.tcl
