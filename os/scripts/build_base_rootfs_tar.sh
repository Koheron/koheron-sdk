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

WORKDIR="${WORKDIR:-$PWD/tmp}"
mkdir -p "$WORKDIR"
root_dir="$(mktemp -d "$WORKDIR/BASE.XXXXXXXXXX")"

cleanup() {
  set +e
  umount -l  "$root_dir/run" 2>/dev/null || true
  umount -R  "$root_dir/dev" 2>/dev/null || true
  umount -R  "$root_dir/sys" 2>/dev/null || true
  umount -l  "$root_dir/proc" 2>/dev/null || true
  rmdir "$root_dir" 2>/dev/null || true
}
trap cleanup EXIT INT TERM

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

# run chroot payload under qemu explicitly (inline path, no qemu_inside var)
chroot "$root_dir" "/usr/bin/$(basename "$qemu_path")" /bin/bash -lc \
  "export DEBIAN_FRONTEND=noninteractive LANG=C LC_ALL=C TIMEZONE='$TIMEZONE' PASSWD='$PASSWD'; /bin/bash /chroot.sh"

# 3) unmount and pack the base
umount -l "$root_dir/run" 2>/dev/null || true
umount -R "$root_dir/dev" 2>/dev/null || true
umount -R "$root_dir/sys" 2>/dev/null || true
umount -l "$root_dir/proc" 2>/dev/null || true

# Make sure the output directory exists, and turn it into an absolute path
out="$BASE_ROOTFS_TAR"
out_dir="$(dirname "$out")"
mkdir -p "$out_dir"
out_abs="$(cd "$out_dir" && pwd)/$(basename "$out")"

# Now pack using an absolute path (works no matter what our cwd is)
tar -C "$root_dir" -czf "$out_abs" .