#!/usr/bin/env bash
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
repo=$(cd "$here/../../../.." && pwd)
out="$repo/tmp/tests/alpha250-dpll/gain-control"
mkdir -p "$out"
"${DPLL_TEST_CXX:-g++}" -std=c++20 -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-omit-frame-pointer -g \
    "$here/test_gain_control.cpp" -o "$out/test_gain_control"
"$out/test_gain_control"
NODE_PATH=${NODE_PATH:-"$repo/web/node_modules"} node --test "$here/test_web.cjs"
