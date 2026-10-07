#!/usr/bin/env bash
set -euo pipefail
repo=$(cd "$(dirname "$0")/../../../.." && pwd)
source "$repo/server/drivers/phase-noise/tests/host-test-env.sh"
flags+=(-Wall -Wextra -Werror)
here=examples/alpha250/dpll/tests
out=tmp/tests/alpha250-dpll/gain-control
mkdir -p "$out"
for name in gain_control p_path_control; do
    "$PNA_TEST_CXX" "${flags[@]}" "$here/test_$name.cpp" -o "$out/test_$name"
    "$out/test_$name"
done
"$PNA_TEST_CXX" -Iexamples/alpha250/phase-noise-analyzer/tests/stubs "${flags[@]}" \
  examples/alpha250/dpll/tests/test_monitor_core.cpp server/external_libs/pffft/pffft.cpp \
  -o tmp/tests/alpha250-dpll/monitor-core
tmp/tests/alpha250-dpll/monitor-core
"$PNA_TEST_CXX" -Iexamples/alpha250/dpll/tests/dma-stubs \
  -Iexamples/alpha250/phase-noise-analyzer/tests/stubs "${flags[@]}" \
  examples/alpha250/dpll/tests/test_dma_guard.cpp -o tmp/tests/alpha250-dpll/dma-guard
tmp/tests/alpha250-dpll/dma-guard
