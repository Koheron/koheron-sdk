#!/usr/bin/env bash
set -euo pipefail
repo=$(cd "$(dirname "$0")/../../../.." && pwd)
export PNA_TEST_CXX=${PNA_TEST_CXX:-${DPLL_TEST_CXX:-g++-13}}
exec bash "$repo/server/drivers/phase-noise/tests/run-host.sh" dpll
