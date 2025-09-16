set -Eeuo pipefail
IFS=$'\n\t'

tmp_project_path=$1
os_path=$2
tmp_os_path=$3
name=$4
os_version_file=$5
zynq_type=$6
image=$tmp_project_path/${name}-development.img
BOOTPART=$7
root_tar_path=$8
overlay_tar=$9
qemu_path=${10:-/usr/bin/qemu-arm-static}
size=1024

# --- robust cleanup & ownership on any exit (success, error, Ctrl-C) ---
device=""            # will be set later by losetup
boot_dir=""          # set by mktemp
root_dir=""          # set by mktemp

cleanup() {
  # preserve original exit code
  rc=$?
  set +e

  # Unmount only if mounted
  [ -n "$root_dir" ] && mountpoint -q "$root_dir/run"  && umount -l "$root_dir/run"
  [ -n "$root_dir" ] && mountpoint -q "$root_dir/dev"  && umount -R "$root_dir/dev"
  [ -n "$root_dir" ] && mountpoint -q "$root_dir/sys"  && umount -R "$root_dir/sys"
  [ -n "$root_dir" ] && mountpoint -q "$root_dir/proc" && umount -l "$root_dir/proc"
  [ -n "$boot_dir" ] && mountpoint -q "$boot_dir"      && umount "$boot_dir"
  [ -n "$root_dir" ] && mountpoint -q "$root_dir"      && umount "$root_dir"

  # Detach loop if set
  if [ -n "$device" ]; then
    losetup -d "$device" 2>/dev/null || true
  fi

  # Remove temp dirs
  [ -n "$boot_dir" ] && rmdir "$boot_dir" 2>/dev/null || true
  [ -n "$root_dir" ] && rmdir "$root_dir" 2>/dev/null || true

  # Make outputs owned by host user even on interruption
  if [ -n "${HOST_UID:-}" ] && [ -n "${HOST_GID:-}" ]; then
    img_dir="$(dirname "${image}")"
    img_base="$(basename "${image}")"
    chown "${HOST_UID}:${HOST_GID}" \
      "${image}" "${image}.sha256" \
      "${img_dir}/release.zip" \
      2>/dev/null || true
    # Optional: also take ownership of build dirs
    chown -R "${HOST_UID}:${HOST_GID}" \
      "${tmp_os_path}" \
      "${tmp_project_path}" \
      2>/dev/null || true
  fi

  exit $rc
}

# Fire cleanup on any exit and on Ctrl-C/TERM
trap 'cleanup' EXIT
trap 'exit 130' INT TERM

linux_image=kernel.itb

dd if=/dev/zero of=$image bs=1M count=${size}

device="$(losetup --find --show "$image")"

boot_dir=`mktemp -d /tmp/BOOT.XXXXXXXXXX`
root_dir=`mktemp -d /tmp/ROOT.XXXXXXXXXX`

passwd=changeme
timezone=Europe/Paris

# Create partitions

parted -s $device mklabel msdos
parted -s $device mkpart primary fat16 4MB 16MB
parted -s $device mkpart primary ext4 16MB 100%

partprobe "$device" || true
udevadm settle || true
sleep 0.3

[ -b "${device}p1" ] || { echo "No ${device}p1 found"; exit 1; }
[ -b "${device}p2" ] || { echo "No ${device}p2 found"; exit 1; }

boot_dev="/dev/$(lsblk -ln -o NAME -x NAME "$device" | sed '2!d')"
root_dev="/dev/$(lsblk -ln -o NAME -x NAME "$device" | sed '3!d')"

# Create file systems

mkfs.vfat -F 16 -n BOOT "$boot_dev"
mkfs.ext4 -F -L rootfs -O metadata_csum,64bit \
         -E lazy_itable_init=1,lazy_journal_init=1 "$root_dev"
tune2fs -m 0 -c 0 -i 6m "$root_dev"

# Mount file systems

mount $boot_dev $boot_dir
mount $root_dev $root_dir

# --- extract base rootfs tarball prepared by Make ---
if [ ! -s "$root_tar_path" ]; then
  echo "Missing or empty rootfs tar: $root_tar_path" >&2
  exit 1
fi
tar -xzf "$root_tar_path" -C "$root_dir"

# Sanity: ensure /etc & /usr/bin are directories
mkdir -p "$root_dir/etc" "$root_dir/usr/bin"

# Ensure the boot FS is actually mounted, then create extlinux dir
mountpoint -q "$boot_dir" || { echo "Boot partition not mounted"; exit 1; }
mkdir -p "$boot_dir/extlinux"

# Copy boot artifacts
cp "$tmp_os_path/boot.bin" "$tmp_os_path/$linux_image" "$boot_dir"

ROOTUUID=$(blkid -s PARTUUID -o value "$root_dev")
BOOTUUID=$(blkid -s PARTUUID -o value "$boot_dev")

cat > "$boot_dir/extlinux/extlinux.conf" <<EOF
DEFAULT Linux
TIMEOUT 3
MENU TITLE Koheron Boot

LABEL Linux
  KERNEL /kernel.itb
  APPEND console=ttyPS0,115200n8 root=PARTUUID=${ROOTUUID} ro rootwait rootfstype=ext4 fsck.repair=yes
EOF

sync

# Add missing configuration files and packages

install -D -m 0644 /etc/resolv.conf "$root_dir/etc/resolv.conf"
install -D -m 0755 "$qemu_path"     "$root_dir/usr/bin/$(basename "$qemu_path")"

# Apply prebuilt overlay
tar -C "$root_dir" -xf "$overlay_tar"

# --- mount pseudo-filesystems for chrooted operations ---
mount -t proc proc "$root_dir/proc"
mount --rbind /sys  "$root_dir/sys"  && mount --make-rslave "$root_dir/sys"
mount --rbind /dev  "$root_dir/dev"  && mount --make-rslave "$root_dir/dev"
mount --bind  /run  "$root_dir/run"  || true  # optional, helps some postinsts

chroot "$root_dir" <<- EOF_CHROOT
export DEBIAN_FRONTEND=noninteractive
export LANG=C
export LC_ALL=C

# ---- PATH for login shells ----
cat >/etc/environment <<'EOF'
PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/local/koheron-server"
EOF

# ---- Faster/leaner apt & dpkg ----
# 1) Avoid fsyncs during dpkg (big speed-up on SD/loops)
cat > /etc/dpkg/dpkg.cfg.d/02_nofsync <<'EOF_DPKG_IO'
force-unsafe-io
EOF_DPKG_IO
# 2) Don’t download docs/manpages/locales (except en_US)
cat > /etc/dpkg/dpkg.cfg.d/01_nodoc <<'EOF_NODOC'
path-exclude=/usr/share/doc/*
path-exclude=/usr/share/man/*
path-exclude=/usr/share/info/*
path-exclude=/usr/share/locale/*
path-include=/usr/share/locale/en/*
path-include=/usr/share/locale/en_US/*
EOF_NODOC
# 3) Don’t fetch translation indexes
cat > /etc/apt/apt.conf.d/99nolangs <<'EOF_NOLANGS'
Acquire::Languages "none";
EOF_NOLANGS
# 4) No recommends/suggests by default
cat > /etc/apt/apt.conf.d/99norecommends <<'EOF_APT'
APT::Install-Recommends "0";
APT::Install-Suggests "0";
EOF_APT
# 5) Be resilient in automation
cat > /etc/apt/apt.conf.d/99conf <<'EOF_CONF'
Dpkg::Options { "--force-confdef"; "--force-confold"; };
Acquire::Retries "3";
EOF_CONF

# ---- fstab (PARTUUID-based) ----
cat > /etc/fstab <<EOF_FSTAB
# <fs>                    <mount>  <type>  <opts>                                                  <dump> <pass>
PARTUUID=${ROOTUUID}      /        ext4    defaults,noatime,lazytime,commit=30,errors=remount-ro   0      1
PARTUUID=${BOOTUUID}      /boot    vfat    ro,noatime,umask=022                                    0      0
tmpfs                     /tmp     tmpfs   mode=1777,nosuid,nodev                                  0      0
tmpfs                     /var/log tmpfs   size=16M,mode=0755,nosuid,nodev,noexec                  0      0
EOF_FSTAB

# ---- basic identity ----
printf '%s\n' 'ttyPS0' >> /etc/securetty
echo koheron > /etc/hostname
cat >> /etc/hosts <<'EOF_HOSTS'
127.0.0.1    localhost.localdomain localhost
127.0.1.1    koheron
EOF_HOSTS

# ---- apt sources (Noble on ARM uses ports.ubuntu.com) ----
cat > /etc/apt/sources.list <<'EOF_SOURCES'
deb http://ports.ubuntu.com/ubuntu-ports noble main restricted universe multiverse
deb http://ports.ubuntu.com/ubuntu-ports noble-updates main restricted universe multiverse
deb http://ports.ubuntu.com/ubuntu-ports noble-security main restricted universe multiverse
# deb http://ports.ubuntu.com/ubuntu-ports noble-backports main restricted universe multiverse
EOF_SOURCES
rm -f /etc/apt/sources.list.d/ubuntu.sources

# ---- /dev/null can be missing in a fresh chroot tarball ----
rm -f /dev/null && mknod /dev/null c 1 3 && chmod 666 /dev/null

chmod +x /usr/local/sbin/grow-rootfs-once

apt-get update
# install it first (without eatmydata), then use it for everything else
apt-get -yq install --no-install-recommends locales eatmydata

locale-gen en_US.UTF-8
update-locale LANG=en_US.UTF-8
echo "$timezone" > /etc/timezone
dpkg-reconfigure --frontend=noninteractive tzdata

# now use eatmydata directly
eatmydata apt-get -yq install --no-install-recommends \
  systemd systemd-sysv \
  openssh-server usbutils psmisc lsof parted curl less nano iw ntfs-3g \
  cloud-guest-utils e2fsprogs bash-completion unzip udev net-tools netbase \
  lsb-base sudo rsync kmod gcc nginx \
  python3-numpy python3-flask uwsgi-core uwsgi-plugin-python3 python3-simplejson \
  systemd-timesyncd systemd-resolved iproute2

rm -f /etc/network/interfaces /etc/network/interfaces.d/* || true

install -d -m0755 /etc/systemd/network
cat >/etc/systemd/network/10-end0.network <<'EOF'
[Match]
Name=end0

[Network]
DHCP=ipv4

[DHCPv4]
UseDNS=true
EOF

# Hook resolv.conf to systemd-resolved stub
rm -f /etc/resolv.conf
ln -s ../run/systemd/resolve/stub-resolv.conf /etc/resolv.conf

systemd-sysusers || true
systemd-tmpfiles --create || true


# ---- Enable services (create symlinks even in chroot) ----
systemctl enable uwsgi || true
systemctl enable grow-rootfs-once.service || true
systemctl enable unzip-default-instrument || true
# systemctl enable koheron-server || true
systemctl enable nginx || true
systemctl enable systemd-networkd.service
systemctl enable systemd-resolved.service
systemctl enable systemd-timesyncd.service
systemctl enable systemd-networkd-wait-online.service || true

systemctl disable getty@tty2.service getty@tty3.service getty@tty4.service getty@tty5.service getty@tty6.service
systemctl mask apt-daily.timer apt-daily-upgrade.timer dpkg-db-backup.timer

# SSH policy (you currently allow root login; consider using a non-root user instead)
sed -i 's/#\?PermitRootLogin.*/PermitRootLogin yes/' /etc/ssh/sshd_config

eatmydata apt-get clean
rm -rf /var/lib/apt/lists/*

# Root password
echo "root:$passwd" | chpasswd

# Hygiene
rm -f /root/.bash_history /root/.ash_history /root/.python_history /root/.lesshst || true

EOF_CHROOT

umount -l "$root_dir/run" 2>/dev/null || true
umount -R  "$root_dir/dev" 2>/dev/null || true
umount -R  "$root_dir/sys" 2>/dev/null || true
umount -l  "$root_dir/proc" 2>/dev/null || true

install -d "$root_dir/etc/nginx" \
          "$root_dir/etc/nginx/sites-available" \
          "$root_dir/etc/nginx/sites-enabled"
install -m 0644 "$os_path/config/nginx.conf" \
                 "$root_dir/etc/nginx/nginx.conf"
install -m 0644 "$os_path/config/nginx-server.conf" \
                 "$root_dir/etc/nginx/sites-available/koheron.conf"
ln -sf ../sites-available/koheron.conf \
       "$root_dir/etc/nginx/sites-enabled/koheron.conf"
rm -f "$root_dir/etc/nginx/sites-enabled/default"
install -D -m 0644 "$os_path/systemd/nginx.service" \
                    "$root_dir/etc/systemd/system/nginx.service"

#rm $root_dir/etc/resolv.conf
rm -f "$root_dir/usr/bin/qemu-a*" 2>/dev/null || true

# Unmount file systems
umount "$boot_dir"
umount "$root_dir"

e2fsck -p -f "$root_dev" >/dev/null 2>&1 || {
  rc=$?
  if [ "$rc" -ne 1 ] && [ "$rc" -ne 0 ]; then
    echo "e2fsck reported a serious error (exit $rc)" >&2
    exit "$rc"
  fi
}

zerofree "$root_dev" >/dev/null 2>&1 || true

img_dir="$(dirname "$image")"
img_base="$(basename "$image")"

(
  cd "$img_dir"
  sha256sum "$img_base" > "${img_base}.sha256"
  zip release.zip "$img_base" "${img_base}.sha256"
)

