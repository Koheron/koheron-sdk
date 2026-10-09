#!/usr/bin/env bash
set -euo pipefail
suite=${1:-all}
case "$suite" in all|pna|fft|phase-modulator|alpha15|alpha250|alpha250-4|dpll) ;; *) echo "Unknown suite: $suite" >&2; exit 2 ;; esac
source "$(dirname "$0")/environment.sh"
node --test web/tests/test_compiler.cjs
node --test web/dds-frequency/tests/test_controls.cjs
node --test web/clock-generator/tests/test_clock.cjs web/plot-basics/tests/test_rendering.cjs \
    web/precision-channels/tests/test_precision_channels.cjs web/power-monitor/tests/test_telemetry.cjs web/instrument/tests/test_poller.cjs
if [[ $suite == all || $suite == pna ]]; then
    bash web/phase-noise/tests/run.sh
elif [[ $suite == alpha250 || $suite == alpha250-4 || $suite == dpll ]]; then
    bash web/phase-noise/tests/run.sh "$suite"
fi
if [[ $suite == all || $suite == fft ]]; then
    bash web/fft/tests/run.sh
elif [[ $suite == alpha15 ]]; then
    bash web/fft/tests/run.sh alpha15
fi
if [[ $suite == all || $suite == phase-modulator ]]; then
    node --test examples/alpha250/phase-modulator/tests/test_web_widget.js
fi
