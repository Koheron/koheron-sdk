#!/usr/bin/env bash
set -euo pipefail

image=${1:?usage: ensure_builder.sh <image> <stamp> <web directory>}
stamp=${2:?}
web_dir=${3:?}

mkdir -p "$(dirname "$stamp")"
stamp_tmp=$(mktemp "$stamp.tmp.XXXXXXXX")
trap 'rm -f "$stamp_tmp"' EXIT
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM

inputs=$(sha256sum "$web_dir/compiler-inputs.mk" "$web_dir/ensure_builder.sh" \
    "$web_dir/Dockerfile.web" "$web_dir/package.json" "$web_dir/package-lock.json" \
    "$web_dir/build.cjs" "$web_dir/transpile.cjs" \
    "$web_dir/plotting/package.json" "$web_dir/plotting/package-lock.json" | sha256sum)
image_id=$(docker image inspect --format '{{.Id}}' "$image" 2>/dev/null || true)

write_settings() {
    printf '%s\n' "IMAGE=$image" "INPUTS=$inputs" "ID=$image_id" > "$stamp_tmp"
}
write_settings
if [[ -z $image_id ]] || ! cmp -s "$stamp_tmp" "$stamp"; then
    docker build -f "$web_dir/Dockerfile.web" -t "$image" "$web_dir"
    image_id=$(docker image inspect --format '{{.Id}}' "$image")
    write_settings
fi
if ! cmp -s "$stamp_tmp" "$stamp"; then
    mv -f "$stamp_tmp" "$stamp"
fi
