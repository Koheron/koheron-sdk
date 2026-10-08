#!/usr/bin/env bash
set -euo pipefail
suite=${1:-all}
case "$suite" in all|alpha15) ;; *) echo "Unknown suite: $suite" >&2; exit 2 ;; esac
source "$(dirname "$0")/../../tests/environment.sh"
node --test web/fft/tests/test_controls.cjs
if [[ $suite == all ]]; then
    for fixture in examples/alpha250/fft/tests/test_web_*.js; do
        node "$fixture"
    done
    node --test examples/alpha250/fft/tests/test_signal_generator.cjs
fi
node --test examples/alpha15/signal-analyzer/tests/test_workspace.cjs
if [[ $suite == all ]]; then
    node --test examples/alpha250-4/fft/tests/test_workspace.cjs
fi
