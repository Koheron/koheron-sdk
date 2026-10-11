#!/usr/bin/env bash
set -euo pipefail

image=${1:?usage: ensure_builder.sh <image> <stamp> <processor directory>}
stamp=${2:?}
processor_dir=${3:?}

mkdir -p "$(dirname "$stamp")"
stamp_tmp=$(mktemp "$stamp.tmp.XXXXXXXX")
trap 'rm -f "$stamp_tmp"' EXIT
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM

inputs=$(sha256sum "$processor_dir/Dockerfile" "$processor_dir/ensure_builder.sh" | sha256sum)
image_id=$(docker image inspect --format '{{.Id}}' "$image" 2>/dev/null || true)
write_settings() {
    printf '%s\n' "IMAGE=$image" "INPUTS=$inputs" "ID=$image_id" > "$stamp_tmp"
}
write_settings
if [[ -z $image_id ]] || ! cmp -s "$stamp_tmp" "$stamp"; then
    docker build -t "$image" "$processor_dir"
    image_id=$(docker image inspect --format '{{.Id}}' "$image")
    write_settings
fi
if ! cmp -s "$stamp_tmp" "$stamp"; then
    mv -f "$stamp_tmp" "$stamp"
fi
