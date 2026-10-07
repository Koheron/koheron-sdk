#!/usr/bin/env bash
set -euo pipefail
repo=$(cd "$(dirname "$0")/../../../.." && pwd)
export PNA_TEST_MODE=${PNA_TEST_MODE:-docker}
exec bash "$repo/server/drivers/phase-noise/tests/run-host.sh" alpha250-4
