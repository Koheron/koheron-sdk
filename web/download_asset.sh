#!/usr/bin/env bash
set -euo pipefail

destination=${1:?usage: download_asset.sh <destination> <url>}
url=${2:?usage: download_asset.sh <destination> <url>}

mkdir -p -- "$(dirname "$destination")"
download=$(mktemp "${destination}.download.XXXXXXXX")
trap 'rm -f -- "$download"' EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

curl -fsSL --retry 3 "$url" -o "$download"
test -s "$download" || { echo "Empty asset download: $url" >&2; exit 1; }
chmod 0644 "$download"
mv -f -- "$download" "$destination"
