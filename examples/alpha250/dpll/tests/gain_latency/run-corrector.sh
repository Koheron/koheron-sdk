#!/usr/bin/env bash
set -euo pipefail
if [[ -n ${DPLL_VIVADO_SETTINGS:-} ]]; then
    set +u
    source "$DPLL_VIVADO_SETTINGS"
    set -u
fi
here=$(cd "$(dirname "$0")" && pwd)
repo=$(cd "$here/../../../../.." && pwd)
out=${DPLL_GAIN_BENCH_OUT:-"$repo/tmp/tests/alpha250-dpll/table-corrector"}
mkdir -p "$out"
cd "$out"
xvlog --sv "$here/../../table_gain.v" "$here/../../table_corrector.v" "$here/corrector_table_tb.v" > compile.log 2>&1
xelab work.corrector_table_test -s corrector_table_test > elaborate.log 2>&1
xsim corrector_table_test -runall > simulation.log 2>&1
rg 'checks passed|Fast correction impulse|Two-cycle gain impulse|Mixed gain impulse|Fatal:|ERROR:' simulation.log || true
rg -q 'Table corrector checks passed' simulation.log
if rg -q 'Fatal:|ERROR:|FATAL:' simulation.log; then exit 1; fi
