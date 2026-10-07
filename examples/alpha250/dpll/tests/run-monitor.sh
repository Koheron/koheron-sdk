#!/usr/bin/env bash
set -euo pipefail
repo=$(cd "$(dirname "$0")/../../../.." && pwd)
PNA_TEST_MODE=${PNA_TEST_MODE:-docker} bash "$repo/examples/alpha250/dpll/tests/run-host.sh"
set +u
source "${DPLL_VIVADO_SETTINGS:-/tools/Xilinx/2025.1/Vivado/settings64.sh}"
set -u
vivado -mode batch -nolog -nojournal -notrace \
    -source "$repo/examples/alpha250/dpll/tests/test_monitor_cdc.tcl"
