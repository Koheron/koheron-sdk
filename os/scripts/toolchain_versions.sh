#!/usr/bin/env bash
set -euo pipefail

report_tool() {
    local tool=$1 option=$2 path=$3 output version build
    # Keep the SDK error trap out of vendor launchers and bound startup time.
    if ! output=$(timeout 10s env -u BASH_ENV "$path/bin/$tool" "$option" 2>&1); then
        output=''
    fi
    read -r version build < <(awk -v product="$tool" '
        { sub(/^[ *\t]+/, "") }
        tolower($1) == product && $2 ~ /^v[0-9]{4}\.[0-9]+(\.[0-9]+)*$/ {
            version = substr($2, 2)
        }
        $1 == "SW" && $2 == "Build" && $3 ~ /^[0-9]+$/ { build = $3 }
        END { print version ? version : "unknown", version && build ? build : "unknown" }
    ' <<< "$output")
    if [[ $version == unknown || $build == unknown ]]; then
        echo "[WARN] $tool version or software build unavailable; recording unknown" >&2
    fi
    printf '%s=%s\n%s_build=%s\n' "$tool" "$version" "$tool" "$build"
}

report_tool vivado -version "$1"
report_tool vitis --version "$2"
