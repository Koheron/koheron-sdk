#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 3 ]; then
  echo "Usage: $0 CHECKSUMS DESTINATION URL" >&2
  exit 2
fi

checksums=$1
destination=$2
url=$3
archive=${destination##*/}

mapfile -t matches < <(awk -v archive="$archive" '$2 == archive { print $1 }' "$checksums")
if [ "${#matches[@]}" -ne 1 ] || [[ ! ${matches[0]} =~ ^[0-9a-f]{64}$ ]]; then
  echo "No unique SHA-256 checksum for $archive in $checksums" >&2
  exit 1
fi
expected=${matches[0]}

verify() {
  local actual
  actual=$(sha256sum "$1")
  actual=${actual%% *}
  [ "$actual" = "$expected" ]
}

if [ -f "$destination" ] && verify "$destination"; then
  echo "Verified $destination"
  exit 0
fi

mkdir -p "$(dirname "$destination")"
download=$(mktemp "${destination}.download.XXXXXX")
trap 'rm -f "$download"' EXIT
curl -fsSL --retry 3 "$url" -o "$download"
if ! verify "$download"; then
  echo "SHA-256 mismatch for $archive (expected $expected)" >&2
  exit 1
fi
mv -f "$download" "$destination"
echo "Verified $destination"
