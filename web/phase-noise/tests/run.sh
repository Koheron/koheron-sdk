#!/usr/bin/env bash
set -euo pipefail
repo=$(cd "$(dirname "$0")/../../.." && pwd)
cd "$repo"
suite=${1:-all}
case "$suite" in all|alpha250|alpha250-4|dpll) ;; *) echo "Unknown suite: $suite" >&2; exit 2 ;; esac
source "$repo/web/tests/environment.sh"
tests=(web/phase-noise/tests/test_export.cjs web/phase-noise/tests/test_phase_precision.cjs web/phase-noise/tests/test_spectrum.cjs)
if [[ $suite == all || $suite == alpha250 ]]; then
    for name in plot numeric_controls signal_generator sample_rate; do
        tests+=("examples/alpha250/phase-noise-analyzer/tests/test_$name.cjs")
    done
fi
if [[ $suite == all || $suite == alpha250-4 ]]; then
    tests+=(examples/alpha250-4/phase-noise-analyzer/tests/test_workspace.cjs)
    PNA_SAMPLE_RATE_BOARD=alpha250-4 node --test examples/alpha250/phase-noise-analyzer/tests/test_sample_rate.cjs
fi
if [[ $suite == all || $suite == dpll ]]; then
    tests+=(examples/alpha250/dpll/tests/test_web.cjs examples/alpha250/dpll/tests/test_monitor_web.cjs)
fi
node --test "${tests[@]}"
