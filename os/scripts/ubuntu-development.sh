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
size=1024

ubuntu_version=24.04.3
part1=/dev/${BOOTPART}p1
part2=/dev/${BOOTPART}p2

if [ "${zynq_type}" = "zynqmp" ]; then
    echo "Building Ubuntu ${ubuntu_version} rootfs for Zynq-MPSoC..."
    root_tar=ubuntu-base-${ubuntu_version}-base-arm64.tar.gz

    qemu_path=/usr/bin/qemu-aarch64-static
    part1=/dev/mmcblk1p1
    part2=/dev/mmcblk1p2
else
    echo "Building Ubuntu ${ubuntu_version} rootfs for Zynq-7000..."
    root_tar=ubuntu-base-${ubuntu_version}-base-armhf.tar.gz
    qemu_path=/usr/bin/qemu-arm-static
fi

linux_image=kernel.itb

dd if=/dev/zero of=$image bs=1M count=${size}

device=`losetup -f`

losetup ${device} ${image}

boot_dir=`mktemp -d /tmp/BOOT.XXXXXXXXXX`
root_dir=`mktemp -d /tmp/ROOT.XXXXXXXXXX`

root_url=http://cdimage.ubuntu.com/ubuntu-base/releases/${ubuntu_version}/release/$root_tar

passwd=changeme
timezone=Europe/Paris

# Create partitions

parted -s $device mklabel msdos
parted -s $device mkpart primary fat16 4MB 16MB
parted -s $device mkpart primary ext4 16MB 100%

boot_dev=/dev/`lsblk -ln -o NAME -x NAME $device | sed '2!d'`
root_dev=/dev/`lsblk -ln -o NAME -x NAME $device | sed '3!d'`

# Create file systems

mkfs.vfat -F 16 -n BOOT "$boot_dev"
mkfs.ext4 -F -L rootfs -O metadata_csum,64bit \
         -E lazy_itable_init=1,lazy_journal_init=1 "$root_dev"
tune2fs -m 0 -c 0 -i 6m "$root_dev"

# Mount file systems

mount $boot_dev $boot_dir
mount $root_dev $root_dir


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


# Copy Ubuntu Core to the root file system

test -f tmp/$root_tar || curl -L $root_url -o tmp/$root_tar

tar -zxf tmp/$root_tar --directory=$root_dir

# Add missing configuration files and packages

cp /etc/resolv.conf $root_dir/etc/
#cp /usr/bin/qemu-arm-static $root_dir/usr/bin/
cp $qemu_path $root_dir/usr/bin/

# Add Web app
mkdir $root_dir/usr/local/www
cp -a tmp/www/. $root_dir/usr/local/www

cp $os_version_file $root_dir/usr/local/www/

mkdir $root_dir/usr/local/api
cp -a tmp/api/. $root_dir/usr/local/api

# Add Koheron TCP/Websocket Server
mkdir $root_dir/usr/local/koheron-server
cp $os_path/scripts/koheron-server-init.py $root_dir/usr/local/koheron-server/koheron-server-init.py

cp $os_path/systemd/unzip-default-instrument.service $root_dir/etc/systemd/system/unzip-default-instrument.service
cp $os_path/systemd/koheron-server.service $root_dir/etc/systemd/system/koheron-server.service
cp $os_path/systemd/koheron-server-init.service $root_dir/etc/systemd/system/koheron-server-init.service

# uwsgi
mkdir $root_dir/etc/uwsgi
cp $os_path/config/uwsgi.ini $root_dir/etc/uwsgi/uwsgi.ini
cp $os_path/systemd/uwsgi.service $root_dir/etc/systemd/system/uwsgi.service

# grow-rootfs-once
cp $os_path/scripts/grow-rootfs-once $root_dir/usr/local/sbin/grow-rootfs-once
cp $os_path/systemd/grow-rootfs-once.service $root_dir/etc/systemd/system/grow-rootfs-once.service

# Add zips
mkdir $root_dir/usr/local/instruments
cp $os_path/scripts/unzip_default_instrument.sh $root_dir/usr/local/instruments/unzip_default_instrument.sh
echo "${name}.zip" > $root_dir/usr/local/instruments/default
cp ${tmp_project_path}/${name}.zip $root_dir/usr/local/instruments

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
cat > /etc/environment <<'EOF_ENV'
PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games:/usr/local/games:/usr/local/koheron-server"
EOF_ENV

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
tmpfs                     /tmp     tmpfs   mode=1777,nosuid,nodev,noexec                           0      0
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
  openssh-server usbutils psmisc lsof parted curl less nano iw ntfs-3g \
  cloud-guest-utils e2fsprogs bash-completion unzip udev net-tools netbase \
  ifupdown lsb-base isc-dhcp-client sudo rsync kmod gcc nginx \
  python3-numpy python3-flask uwsgi-core uwsgi-plugin-python3 python3-simplejson \
  systemd-timesyncd

systemd-sysusers || true
systemd-tmpfiles --create || true

# ---- Enable services (create symlinks even in chroot) ----
systemctl enable systemd-timesyncd || true
systemctl enable uwsgi || true
systemctl enable grow-rootfs-once.service || true
systemctl enable unzip-default-instrument || true
# systemctl enable koheron-server || true
systemctl enable nginx || true

# SSH policy (you currently allow root login; consider using a non-root user instead)
sed -i 's/#\?PermitRootLogin.*/PermitRootLogin yes/' /etc/ssh/sshd_config

# ---- Basic net config (ifupdown) ----
touch /etc/udev/rules.d/75-persistent-net-generator.rules
cat >> /etc/network/interfaces <<'EOF_IF'
allow-hotplug end0
# DHCP configuration
iface end0 inet dhcp
# Static IP (example)
# iface end0 inet static
#   address 192.168.1.100
#   gateway 192.168.1.1
#   netmask 255.255.255.0
#   network 192.168.1.0
#   broadcast 192.168.1.255
  # koheron-server-init must be first post-up
  post-up systemctl start koheron-server-init
EOF_IF

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
rm $root_dir/usr/bin/qemu-a*

# Unmount file systems
sync
umount "$boot_dir"
umount "$root_dir"
rmdir "$boot_dir" "$root_dir"
e2fsck -f -y "$root_dev" || true
zerofree "$root_dev" >/dev/null 2>&1 || true
losetup -d "$device"

img_dir="$(dirname "$image")"
img_base="$(basename "$image")"

(
  cd "$img_dir"
  sha256sum "$img_base" > "${img_base}.sha256"
  zip release.zip "$img_base" "${img_base}.sha256"
)

# Make outputs owned by the host user if we ran as root in the container
if [ -n "${HOST_UID:-}" ] && [ -n "${HOST_GID:-}" ]; then
  chown ${HOST_UID}:${HOST_GID} "$image" "${image}.sha256" \
    "$(dirname "$image")/release.zip" 2>/dev/null || true
fi