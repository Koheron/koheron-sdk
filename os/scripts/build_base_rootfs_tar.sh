#!/usr/bin/env bash
# Build a fully configured base rootfs tarball (post-chroot), no boot/image deps.
# Usage:
#   build_base_rootfs_tar.sh <root_tar_path> <base_rootfs_tar> <qemu_path>
# Env (optional):
#   TIMEZONE=Europe/Paris  PASSWD=changeme

set -Eeuo pipefail
IFS=$'\n\t'

root_tar_path=${1:?usage: build_base_rootfs_tar.sh <root_tar_path> <base_rootfs_tar> <qemu_path>}
BASE_ROOTFS_TAR=${2:?usage: build_base_rootfs_tar.sh <root_tar_path> <base_rootfs_tar> <qemu_path>}
qemu_path=${3:?usage: build_base_rootfs_tar.sh <root_tar_path> <base_rootfs_tar> <qemu_path>}

TIMEZONE=${TIMEZONE:-Europe/Paris}
PASSWD=${PASSWD:-changeme}

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CHROOT_PAYLOAD="$SCRIPT_DIR/chroot_base_rootfs.sh"
FINALIZE_ROOTFS="$SCRIPT_DIR/finalize_rootfs.sh"

WORKDIR="${WORKDIR:-$PWD/tmp}"
mkdir -p "$WORKDIR"
root_dir="$(mktemp -d "$WORKDIR/BASE.XXXXXXXXXX")"
out_tmp=""

unmount_rootfs() {
  local target failed=0
  for target in run dev sys proc; do
    if mountpoint -q "$root_dir/$target"; then
      umount -R "$root_dir/$target" || failed=1
    fi
  done
  return "$failed"
}

cleanup() {
  local status=$?
  set +e
  if unmount_rootfs; then
    rm -rf --one-file-system -- "$root_dir" || {
      echo "Could not remove build tree at $root_dir" >&2
      [ "$status" -ne 0 ] || status=1
    }
  else
    echo "Rootfs mounts remain; keeping build tree at $root_dir" >&2
    [ "$status" -ne 0 ] || status=1
  fi
  [ -z "$out_tmp" ] || rm -f -- "$out_tmp"
  exit "$status"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

# 1) lay down minimal rootfs
[ -s "$root_tar_path" ] || { echo "Missing or empty rootfs tar: $root_tar_path" >&2; exit 1; }
tar -xzf "$root_tar_path" -C "$root_dir"
chown 0:0 "$root_dir"
mkdir -p "$root_dir/etc" "$root_dir/usr/bin"

# essentials for chroot
install -D -m0644 /etc/resolv.conf "$root_dir/etc/resolv.conf"
install -D -m0755 "$qemu_path" "$root_dir/usr/bin/$(basename "$qemu_path")"

# 2) mount pseudo-fs and run chroot via qemu (no binfmt needed)
mount -t proc proc "$root_dir/proc"
mount --rbind /sys "$root_dir/sys" && mount --make-rslave "$root_dir/sys"
mount --rbind /dev "$root_dir/dev" && mount --make-rslave "$root_dir/dev"
mount --bind  /run "$root_dir/run" || true

install -D -m0755 "$CHROOT_PAYLOAD" "$root_dir/chroot.sh"

# ensure the helper we just copied exists
if [ ! -x "$root_dir/usr/bin/$(basename "$qemu_path")" ]; then
  echo "qemu helper missing inside chroot: /usr/bin/$(basename "$qemu_path")" >&2
  exit 1
fi

# Run chroot payload under qemu explicitly (avoid login shell to skip profile scripts).
chroot "$root_dir" "/usr/bin/$(basename "$qemu_path")" /bin/bash --noprofile --norc -c \
  "export DEBIAN_FRONTEND=noninteractive LANG=C LC_ALL=C TIMEZONE='$TIMEZONE' PASSWD='$PASSWD'; /bin/bash /chroot.sh"

# 3) unmount and pack the base
unmount_rootfs
bash "$FINALIZE_ROOTFS" "$root_dir" "$(basename "$qemu_path")"

# Make sure the output directory exists, and turn it into an absolute path
out="$BASE_ROOTFS_TAR"
out_dir="$(dirname "$out")"
mkdir -p "$out_dir"
out_abs="$(cd "$out_dir" && pwd)/$(basename "$out")"

# Publish only a complete archive; leave any existing cache intact on failure.
out_tmp=$(mktemp "${out_abs}.tmp.XXXXXXXX")
tar -C "$root_dir" -czf "$out_tmp" .
chmod 0644 "$out_tmp"
rm -rf --one-file-system -- "$root_dir"
mv -f -- "$out_tmp" "$out_abs"
out_tmp=""
