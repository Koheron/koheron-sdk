#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/../../tests/environment.sh"
for fixture in examples/alpha250/fft/tests/test_web_*.js; do
    node "$fixture"
done
node --test examples/alpha250/fft/tests/test_signal_generator.cjs \
    examples/alpha250/fft/tests/test_precision_channels.cjs \
    examples/alpha15/signal-analyzer/tests/test_workspace.cjs
