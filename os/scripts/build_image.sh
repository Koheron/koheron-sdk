#!/usr/bin/env bash
set -Eeuo pipefail
IFS=$'\n\t'

# --- Args (same order as your Make target; root_tar_path kept for argv shape) ---
tmp_project_path=$1
os_path=$2
tmp_os_path=$3
root_tar_path=$4          # (unused here; base comes from BASE_ROOTFS_TAR)
overlay_tar=$5
qemu_path=${6:-/usr/bin/qemu-arm-static}
release_name=$7

image="$tmp_project_path/${release_name}.img"
size=1024

# --- Required: fully-configured base rootfs cache (post-chroot, pre-overlay) ---
# Build it with build_base_rootfs_tar.sh first, then pass via env from Make:
#   env BASE_ROOTFS_TAR=".../.cache/base-rootfs-<mode>.tgz"
BASE_ROOTFS_TAR="${BASE_ROOTFS_TAR:?BASE_ROOTFS_TAR must be set (run: make base-rootfs)}"

linux_image=kernel.itb
passwd=changeme
timezone=Europe/Paris

# --- robust cleanup & ownership on any exit (success, error, Ctrl-C) ---
device=""            # will be set later by losetup
boot_dir=""          # set by mktemp
root_dir=""          # set by mktemp

cleanup() {
  # preserve original exit code
  rc=$?
  set +e

  # Unmount filesystems if mounted
  [ -n "$boot_dir" ] && mountpoint -q "$boot_dir" && umount "$boot_dir"
  [ -n "$root_dir" ] && mountpoint -q "$root_dir" && umount "$root_dir"

  # Detach loop if set
  if [ -n "$device" ]; then
    losetup -d "$device" 2>/dev/null || true
  fi

  # Remove temp dirs
  [ -n "$boot_dir" ] && rmdir "$boot_dir" 2>/dev/null || true
  [ -n "$root_dir" ] && rmdir "$root_dir" 2>/dev/null || true

  # Make outputs owned by host user even on interruption
  if [ -n "${HOST_UID:-}" ] && [ -n "${HOST_GID:-}" ]; then
    chown "${HOST_UID}:${HOST_GID}" \
      "${tmp_project_path}/${release_name}.img" \
      "${tmp_project_path}/${release_name}.img.sha256" \
      "${tmp_project_path}/${release_name}.zip" \
      "${tmp_project_path}/manifest-${release_name}.txt" \
      2>/dev/null || true
    chown -R "${HOST_UID}:${HOST_GID}" \
      "${tmp_os_path}" "${tmp_project_path}" \
      2>/dev/null || true
  fi

  exit $rc
}

# Fire cleanup on any exit and on Ctrl-C/TERM
trap 'cleanup' EXIT
trap 'exit 130' INT TERM

# --- create raw image & loop device ---
dd if=/dev/zero of="$image" bs=1M count=0 seek="${size}"
device="$(losetup --find --show "$image")"

# NOTE: if your builder mounts /tmp as noexec, ensure Docker uses --tmpfs /tmp:exec
boot_dir=$(mktemp -d /tmp/BOOT.XXXXXXXXXX)
root_dir=$(mktemp -d /tmp/ROOT.XXXXXXXXXX)

# --- Create partitions ---
parted -s "$device" mklabel msdos
parted -s "$device" mkpart primary fat16 4MB 16MB
parted -s "$device" mkpart primary ext4 16MB 100%

partprobe "$device" || true
udevadm settle || true
sleep 0.3

[ -b "${device}p1" ] || { echo "No ${device}p1 found"; exit 1; }
[ -b "${device}p2" ] || { echo "No ${device}p2 found"; exit 1; }

boot_dev="/dev/$(lsblk -ln -o NAME -x NAME "$device" | sed '2!d')"
root_dev="/dev/$(lsblk -ln -o NAME -x NAME "$device" | sed '3!d')"

# --- Create file systems ---
mkfs.vfat -F 16 -n BOOT "$boot_dev"
mkfs.ext4 -F -L rootfs -O metadata_csum,64bit \
  -E lazy_itable_init=1,lazy_journal_init=1 "$root_dev"
tune2fs -m 0 -c 0 -i 6m "$root_dev"

# --- Mount file systems ---
mount "$boot_dev" "$boot_dir"
mount "$root_dev" "$root_dir"

# --- Copy fully-configured base rootfs (from cache) ---
[ -s "$BASE_ROOTFS_TAR" ] || { echo "Missing base rootfs cache: $BASE_ROOTFS_TAR" >&2; exit 1; }
tar -xzf "$BASE_ROOTFS_TAR" -C "$root_dir"
chown 0:0 "$root_dir"

# Sanity: ensure /etc & /usr/bin are directories
mkdir -p "$root_dir/etc" "$root_dir/usr/bin"

# --- Boot partition content (extlinux) ---
mountpoint -q "$boot_dir" || { echo "Boot partition not mounted"; exit 1; }
mkdir -p "$boot_dir/extlinux"

# Copy boot artifacts
cp "$tmp_os_path/boot.bin" "$tmp_os_path/$linux_image" "$boot_dir"

ROOTUUID=$(blkid -s PARTUUID -o value "$root_dev")
BOOTUUID=$(blkid -s PARTUUID -o value "$boot_dev")

cat > "$boot_dir/extlinux/extlinux.conf" <<EOF
DEFAULT Linux
TIMEOUT 1
MENU TITLE Koheron Boot (${release_name})

LABEL Linux
  KERNEL /kernel.itb
  APPEND console=ttyPS0,115200n8 root=PARTUUID=${ROOTUUID} ro rootwait rootfstype=ext4 fsck.repair=yes
EOF

sync

# --- Early essentials inside rootfs for any post-overlay chroot ---
install -D -m0644 /etc/resolv.conf "$root_dir/etc/resolv.conf"
install -D -m0755 "$qemu_path"     "$root_dir/usr/bin/$(basename "$qemu_path")"

# --- fstab (PARTUUID-based) generated outside chroot, always fresh ---
cat > "$root_dir/etc/fstab" <<EOF_FSTAB
# <fs>                    <mount>  <type>  <opts>                                                  <dump> <pass>
PARTUUID=${ROOTUUID}      /        ext4    defaults,noatime,lazytime,commit=30,errors=remount-ro   0      1
PARTUUID=${BOOTUUID}      /boot    vfat    ro,noatime,umask=022                                    0      0
tmpfs                     /tmp     tmpfs   mode=1777,nosuid,nodev                                  0      0
tmpfs                     /var/log tmpfs   size=16M,mode=0755,nosuid,nodev,noexec                  0      0
EOF_FSTAB

# --- Apply prebuilt overlay ---
tar -C "$root_dir" -xf "$overlay_tar"

# Ensure overlay script is executable if present
[ -e "$root_dir/usr/local/sbin/grow-rootfs-once" ] && chmod 0755 "$root_dir/usr/local/sbin/grow-rootfs-once"

# Runtime/log/state dirs that may live on tmpfs at boot
install -d -m0755 "$root_dir/var/log/nginx"
install -d -m0755 "$root_dir/run/uwsgi"
install -d -m0755 "$root_dir/var/lib/systemd/timesync"

# --- nginx/site config installed from repo (outside chroot) ---
install -d "$root_dir/etc/nginx" \
          "$root_dir/etc/nginx/sites-available" \
          "$root_dir/etc/nginx/sites-enabled"
install -m0644 "$os_path/config/nginx.conf" \
               "$root_dir/etc/nginx/nginx.conf"
install -m0644 "$os_path/config/nginx-server.conf" \
               "$root_dir/etc/nginx/sites-available/koheron.conf"
ln -sfn ../sites-available/koheron.conf \
        "$root_dir/etc/nginx/sites-enabled/koheron.conf"
rm -f "$root_dir/etc/nginx/sites-enabled/default"
install -D -m0644 "$os_path/systemd/nginx.service" \
                  "$root_dir/etc/systemd/system/nginx.service"

# --- (optional) post-overlay tasks inside chroot (via QEMU, no binfmt needed) ---
if [ -f "$os_path/scripts/chroot_overlay.sh" ]; then
  # Minimal pseudo-fs mounts (short-lived, just for overlay tasks)
  mount -t proc proc "$root_dir/proc"
  mount --rbind /sys  "$root_dir/sys"  && mount --make-rslave "$root_dir/sys"
  mount --rbind /dev  "$root_dir/dev"  && mount --make-rslave "$root_dir/dev"
  mount --bind  /run  "$root_dir/run"  || true

  install -D -m0755 "$os_path/scripts/chroot_overlay.sh" "$root_dir/chroot_overlay.sh"
  chroot "$root_dir" "/usr/bin/$(basename "$qemu_path")" /bin/bash -lc "/bin/bash /chroot_overlay.sh || true"

  umount -l "$root_dir/run" 2>/dev/null || true
  umount -R  "$root_dir/dev" 2>/dev/null || true
  umount -R  "$root_dir/sys" 2>/dev/null || true
  umount -l  "$root_dir/proc" 2>/dev/null || true
fi

# Remove qemu helper from the target rootfs
rm -f "$root_dir/usr/bin/qemu-a*" 2>/dev/null || true

# --- Unmount file systems ---
umount "$boot_dir"
umount "$root_dir"

# --- shrink rootfs to minimum, leave 32 MiB cushion, resize p2, truncate image, refresh loop ---

# accept e2fsck rc 0/1 ("fixed") as success
fsck_ok() {
  set +e
  e2fsck -f -y "$root_dev"
  rc=$?
  set -e
  case "$rc" in 0|1) return 0 ;; *) echo "e2fsck error (rc=$rc)" >&2; exit "$rc" ;; esac
}

# log original image size
orig_img_bytes=$(stat -c%s "$image" 2>/dev/null || wc -c <"$image")
echo "[shrink] original image: $(numfmt --to=iec --suffix=B "$orig_img_bytes" 2>/dev/null || echo ${orig_img_bytes}B)"

# 1) fsck -> shrink to minimum -> fsck again
fsck_ok
resize2fs -M "$root_dev"
fsck_ok

# 2) gather geometry
eval "$(parted -sm "$device" unit s print | awk -F: '/^2:/{gsub(/s/,"",$2); printf "p2_start=%d;\n", $2+0}')"
blkcnt=$(tune2fs -l "$root_dev" | awk -F': *' '/Block count:/ {print $2}')
blksz=$(tune2fs -l "$root_dev" | awk -F': *' '/Block size:/  {print $2}')
root_bytes=$(( blkcnt * blksz ))

# 3) compute new p2 size (1 MiB align + 32 MiB cushion)
align=2048                 # sectors per MiB (512B sectors)
cushion_mib=32
cushion_sectors=$(( cushion_mib * align ))
need_sectors=$(( (root_bytes + 511) / 512 ))
new_p2_sectors=$(( ( (need_sectors + align - 1) / align ) * align ))
new_p2_sectors=$(( new_p2_sectors + cushion_sectors ))
new_p2_sectors=$(( ( (new_p2_sectors + align - 1) / align ) * align ))
new_p2_end=$(( p2_start + new_p2_sectors - 1 ))

echo "[shrink] fs(min): $(numfmt --to=iec --suffix=B "$root_bytes" 2>/dev/null || echo ${root_bytes}B)"
echo "[shrink] cushion: ${cushion_mib} MiB"
echo "[shrink] p2 -> start=${p2_start} end=${new_p2_end} sectors=${new_p2_sectors} ($(numfmt --to=iec --suffix=B $((new_p2_sectors*512)) 2>/dev/null || echo $((new_p2_sectors*512))B))"

# 4) shrink partition 2 (quiet; don't ask kernel to reread here)
if command -v sfdisk >/dev/null 2>&1; then
  echo ",${new_p2_sectors}" | sfdisk --no-reread -q -N 2 "$device" >/dev/null 2>&1 || true
else
  echo "[shrink] sfdisk not found; falling back to parted"
  parted -s "$device" unit s resizepart 2 "${new_p2_end}" >/dev/null 2>&1 || true
fi

# 5) truncate the image to the end of p2
new_img_bytes=$(( (new_p2_end + 1) * 512 ))
truncate -s "$new_img_bytes" "$image"
saved=$(( orig_img_bytes - new_img_bytes ))
echo "[shrink] new image: $(numfmt --to=iec --suffix=B "$new_img_bytes" 2>/dev/null || echo ${new_img_bytes}B)  (saved $(numfmt --to=iec --suffix=B "$saved" 2>/dev/null || echo ${saved}B))"

# 6) refresh loop mapping so kernel sees new partition table (needed for zerofree)
sync
blockdev --flushbufs "$device" 2>/dev/null || true
losetup -d "$device"
device="$(losetup --find --show -P "$image")"

# recompute p1/p2 nodes
boot_dev="/dev/$(lsblk -ln -o NAME -x NAME "$device" | sed '2!d')"
root_dev="/dev/$(lsblk -ln -o NAME -x NAME "$device" | sed '3!d')"

# sanity: verify the new end
current_end=$(parted -sm "$device" unit s print | awk -F: '/^2:/{gsub(/s/,"",$3); print $3+0}')
[ "$current_end" -eq "$new_p2_end" ] || echo "[shrink] WARN: p2 end mismatch ($current_end != $new_p2_end)"

# now run zerofree (unmounted p2)
zerofree "$root_dev" >/dev/null 2>&1 || true

# --- package (sha + zip) ---
(
  set -Eeuo pipefail
  cd "$tmp_project_path"

  img="${release_name}.img"
  sha="${release_name}.img.sha256"
  manifest="manifest-${release_name}.txt"
  zipfile="${release_name}.zip"

  # Require artifacts to exist (and be non-empty)
  for f in "$img" "$manifest"; do
    if [ ! -s "$f" ]; then
      echo "Missing required artifact: $f" >&2
      exit 1
    fi
  done

  # Create checksum then zip everything (fail on any error)
  sha256sum -- "$img" > "$sha"
  zip -X -9 "$zipfile" "$img" "$sha" "$manifest"
)
