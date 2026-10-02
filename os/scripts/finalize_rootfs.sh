#!/usr/bin/env bash
# Reset cloned identities and remove build-only files from an offline rootfs.
set -Eeuo pipefail

root_dir=$(realpath -e -- "${1:?usage: finalize_rootfs.sh ROOT_DIR QEMU_BASENAME}")
qemu_name=${2:?usage: finalize_rootfs.sh ROOT_DIR QEMU_BASENAME}
[ "$root_dir" != / ] && [ -d "$root_dir/etc" ] && [ -d "$root_dir/usr" ] || {
  echo "Expected an offline rootfs directory, got $root_dir" >&2
  exit 1
}
case "$qemu_name" in
  qemu-arm-static|qemu-aarch64-static) ;;
  *) echo "Unexpected QEMU helper: $qemu_name" >&2; exit 1 ;;
esac

# An empty ID lets systemd generate a board-specific ID while preserving the
# service enablement selected by the image builder (no first-boot presets).
rm -f -- "$root_dir/etc/machine-id" "$root_dir/var/lib/dbus/machine-id"
install -m0644 /dev/null "$root_dir/etc/machine-id"
if [ -d "$root_dir/var/lib/dbus" ]; then
  ln -s /etc/machine-id "$root_dir/var/lib/dbus/machine-id"
fi
rm -f -- "$root_dir"/etc/ssh/ssh_host_* \
  "$root_dir/var/lib/systemd/random-seed" \
  "$root_dir/var/lib/systemd/credential.secret"

# force-unsafe-io is useful during image construction, not on the SD card.
rm -f -- "$root_dir/etc/dpkg/dpkg.cfg.d/02_nofsync" \
  "$root_dir/usr/bin/$qemu_name" \
  "$root_dir/chroot.sh" "$root_dir/chroot_overlay.sh"
