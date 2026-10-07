#!/usr/bin/env bash
set -euo pipefail
repo=$(cd "$(dirname "$0")/../../../.." && pwd)
cd "$repo"
suite=${1:-all}
stage=${2:-all}
case "$suite" in all|alpha250|alpha250-4|dpll) ;; *) echo "Unknown suite: $suite" >&2; exit 2 ;; esac
case "$stage" in all|python|cpp|web) ;; *) echo "Unknown stage: $stage" >&2; exit 2 ;; esac
mode=${PNA_TEST_MODE:-native}
case "$mode" in native|docker) ;; *) echo "PNA_TEST_MODE must be native or docker" >&2; exit 2 ;; esac
if [[ -x .venv/bin/python3 ]]; then
    python=${PNA_PYTHON:-.venv/bin/python3}
else
    python=${PNA_PYTHON:-python3}
fi

run_cpp() {
    if [[ $mode == docker ]]; then
        docker run --rm -u "$(id -u):$(id -g)" -v "$repo:/review" -w /review \
            -e PNA_TEST_CXX="${PNA_TEST_CXX:-g++-13}" \
            -e PNA_TEST_CXXFLAGS="${PNA_TEST_CXXFLAGS:-}" \
            "${PNA_CPP_IMAGE:-cross-armhf:24.04}" bash "$1"
    else
        bash "$1"
    fi
}

run_web() {
    if [[ $mode == docker ]]; then
        docker run --rm -u "$(id -u):$(id -g)" -v "$repo:/review" -w /review \
            "${PNA_WEB_IMAGE:-koheron-web:node20}" "$@"
    else
        "$@"
    fi
}

if [[ $stage == all || $stage == python ]]; then
    if [[ $suite == all || $suite == alpha250 ]]; then
        for test in test_client test_phase_calibration test_phase_modulator; do
            "$python" "examples/alpha250/phase-noise-analyzer/tests/$test.py"
        done
    fi
    if [[ $suite == all || $suite == alpha250-4 ]]; then
        "$python" examples/alpha250-4/phase-noise-analyzer/tests/test_client.py
    fi
    if [[ $suite == all || $suite == dpll ]]; then
        "$python" examples/alpha250/dpll/tests/test_monitor_client.py
    fi
fi

if [[ $stage == all || $stage == cpp ]]; then
    run_cpp server/drivers/phase-noise/tests/run-cpp.sh
    for board in alpha250 alpha250-4; do
        if [[ $suite == all || $suite == "$board" ]]; then
            run_cpp "examples/$board/phase-noise-analyzer/tests/run-cpp.sh"
        fi
    done
    if [[ $suite == all || $suite == dpll ]]; then
        run_cpp examples/alpha250/dpll/tests/run-cpp.sh
    fi
    "$python" server/drivers/phase-noise/tests/check_streaming_welch.py \
        tmp/tests/phase-noise/streaming-welch.bin
    if [[ $suite == all || $suite == alpha250-4 ]]; then
        PNA_TEST_MODE="$mode" "$python" examples/alpha250-4/phase-noise-analyzer/tests/check_calculations.py
    fi
fi

if [[ $stage == all || $stage == web ]]; then
    run_web bash web/phase-noise/tests/run.sh "$suite"
fi

if [[ $stage == all && ( $suite == all || $suite == alpha250-4 ) ]]; then
    # Preserve the compiled-bundle/C++-payload integration check. Build web first.
    run_web node examples/alpha250-4/phase-noise-analyzer/tests/test_web.cjs \
        "${PNA_WEB_BUNDLE:-tmp/examples/alpha250-4/phase-noise-analyzer/web/app.js}" \
        tmp/tests/alpha250-4-phase-noise-analyzer/measurements.bin \
        tmp/tests/alpha250-4-phase-noise-analyzer/tracking.bin
fi
