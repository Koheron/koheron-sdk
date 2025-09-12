set -e

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
    linux_image=Image
    qemu_path=/usr/bin/qemu-aarch64-static
    part1=/dev/mmcblk1p1
    part2=/dev/mmcblk1p2
else
    echo "Building Ubuntu ${ubuntu_version} rootfs for Zynq-7000..."
    root_tar=ubuntu-base-${ubuntu_version}-base-armhf.tar.gz
    linux_image=uImage
    qemu_path=/usr/bin/qemu-arm-static
fi

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

mkfs.vfat -v $boot_dev
mkfs.ext4 -F -j $root_dev

# Mount file systems

mount $boot_dev $boot_dir
mount $root_dev $root_dir

# Copy boot artifacts
cp "$tmp_os_path/boot.bin" "$tmp_os_path/$linux_image" "$tmp_os_path/devicetree.dtb" "$boot_dir"

# Create extlinux menu (U-Boot will auto-scan /extlinux/extlinux.conf)
mkdir -p "$boot_dir/extlinux"
cat > "$boot_dir/extlinux/extlinux.conf" <<EOF
DEFAULT Linux
TIMEOUT 3
MENU TITLE Koheron Boot

LABEL Linux
    KERNEL /${linux_image]
    FDT /devicetree.dtb
    APPEND console=ttyPS0,115200 root=${part2} ro rootfstype=ext4 rootwait
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

# Add zips
mkdir $root_dir/usr/local/instruments
cp $os_path/scripts/unzip_default_instrument.sh $root_dir/usr/local/instruments/unzip_default_instrument.sh
echo "${name}.zip" > $root_dir/usr/local/instruments/default
cp ${tmp_project_path}/${name}.zip $root_dir/usr/local/instruments

chroot $root_dir <<- EOF_CHROOT
export LANG=C
export LC_ALL=C
# Add /usr/local/koheron-server to the environment PATH
cat <<- EOF_CAT > etc/environment
PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games:/usr/local/games:/usr/local/koheron-server"
EOF_CAT
cat <<- EOF_CAT > etc/apt/apt.conf.d/99norecommends
APT::Install-Recommends "0";
APT::Install-Suggests "0";
EOF_CAT
cat <<- EOF_CAT > etc/fstab
# /etc/fstab: static file system information.
# <file system> <mount point>   <type>  <options>           <dump>  <pass>
$part2          /               ext4    rw,noatime          0       1
$part1          /boot           vfat    ro,noatime          0       2
tmpfs           /tmp            tmpfs   defaults,noatime    0       0
tmpfs           /var/log        tmpfs   size=1M,noatime     0       0
EOF_CAT
cat <<- EOF_CAT >> etc/securetty
# Serial Console for Xilinx Zynq-7000
ttyPS0
EOF_CAT
echo koheron > etc/hostname
cat <<- EOF_CAT >> etc/hosts
127.0.0.1    localhost.localdomain localhost
127.0.1.1    koheron
EOF_CAT

# Noble on ARM uses ports.ubuntu.com
cat > /etc/apt/sources.list <<'EOF_SOURCES'
deb http://ports.ubuntu.com/ubuntu-ports noble main restricted universe multiverse
deb http://ports.ubuntu.com/ubuntu-ports noble-updates main restricted universe multiverse
deb http://ports.ubuntu.com/ubuntu-ports noble-security main restricted universe multiverse
# deb http://ports.ubuntu.com/ubuntu-ports noble-backports main restricted universe multiverse
EOF_SOURCES

# Make sure /dev/null exists in the fresh chroot before apt
rm -f /dev/null
mknod /dev/null c 1 3
chmod 666 /dev/null

apt update
apt -y upgrade
apt -y install locales
locale-gen en_US.UTF-8
update-locale LANG=en_US.UTF-8
echo $timezone > etc/timezone
dpkg-reconfigure --frontend=noninteractive tzdata
DEBIAN_FRONTEND=noninteractive apt install -yq ntp
apt install -y openssh-server
apt install -y usbutils psmisc lsof
apt install -y parted curl less vim iw ntfs-3g
apt install -y bash-completion unzip
apt install -y udev net-tools netbase ifupdown network-manager lsb-base isc-dhcp-client
apt install -y ntpdate sudo rsync
apt install -y kmod
apt install -y gcc
apt install -y nginx
apt install -y python3-numpy
apt install -y python3-flask
apt install -y uwsgi-core uwsgi-plugin-python3
apt install -y python3-simplejson

systemctl enable uwsgi
systemctl enable unzip-default-instrument
#systemctl enable koheron-server
systemctl enable nginx
sed -i 's/#PermitRootLogin prohibit-password/PermitRootLogin yes/' /etc/ssh/sshd_config
touch etc/udev/rules.d/75-persistent-net-generator.rules
cat <<- EOF_CAT >> etc/network/interfaces
allow-hotplug end0
# DHCP configuration
iface end0 inet dhcp
# Static IP
#iface end0 inet static
#  address 192.168.1.100
#  gateway 192.168.1.1
#  netmask 255.255.255.0
#  network 192.168.1.0
#  broadcast 192.168.1.255
  # /!\ koheron-server-init must be the first post-up called
  # else it wont be called if a previous post-up fails.
  post-up systemctl start koheron-server-init
  post-up ntpdate -u ntp.u-psud.fr
EOF_CAT
apt clean
echo root:$passwd | chpasswd
history -c
EOF_CHROOT

# nginx
rm $root_dir/etc/nginx/sites-enabled/default
cp $os_path/config/nginx.conf $root_dir/etc/nginx/nginx.conf
cp $os_path/config/nginx-server.conf $root_dir/etc/nginx/sites-enabled/nginx-server.conf
cp $os_path/systemd/nginx.service $root_dir/etc/systemd/system/nginx.service

#rm $root_dir/etc/resolv.conf
rm $root_dir/usr/bin/qemu-a*

# Unmount file systems

umount $boot_dir $root_dir

rmdir $boot_dir $root_dir

zerofree $root_dev

losetup -d $device

img_dir="$(dirname "$image")"
img_base="$(basename "$image")"

(
  cd "$img_dir"
  sha256sum "$img_base" > "${img_base}.sha256"
  zip release.zip "$img_base" "${img_base}.sha256"
)

