set -e

tmp_project_path=$1
os_path=$2
tmp_os_path=$3
name=$4
os_version_file=$5
zynq_type=$6
image=$tmp_project_path/${name}-production.img
BOOTPART=$7
size=1024

ubuntu_version=20.04.2

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
parted -s $device mkpart primary fat16 4MB 512MB
parted -s $device mkpart primary ext4 512MB 100%

boot_dev=/dev/`lsblk -ln -o NAME -x NAME $device | sed '2!d'`
root_dev=/dev/`lsblk -ln -o NAME -x NAME $device | sed '3!d'`

# Create file systems

mkfs.vfat -v $boot_dev
mkfs.ext4 -F -j $root_dev

# Mount file systems

mount $boot_dev $boot_dir
mount $root_dev $root_dir

# Copy files to the boot file system

cp $tmp_os_path/boot.bin $tmp_os_path/devicetree.dtb $tmp_os_path/$linux_image $os_path/uEnv.txt $boot_dir

# Copy Ubuntu Core to the root file system

test -f tmp/$root_tar || curl -L $root_url -o tmp/$root_tar

tar -zxf tmp/$root_tar --directory=$root_dir

# Add missing configuration files and packages

cp /etc/resolv.conf $root_dir/etc/
cp $qemu_path $root_dir/usr/bin/

# Add Koheron TCP/Websocket Server
mkdir $root_dir/usr/local/koheron-server
cp $os_path/scripts/koheron-server-init.py $root_dir/usr/local/koheron-server/koheron-server-init.py

cp $os_path/systemd/unzip-default-instrument.service $root_dir/etc/systemd/system/unzip-default-instrument.service
cp $os_path/systemd/koheron-server.service $root_dir/etc/systemd/system/koheron-server.service
cp $os_path/systemd/koheron-server-init.service $root_dir/etc/systemd/system/koheron-server-init.service

# Add zip
mkdir $root_dir/usr/local/instruments
cp $os_path/scripts/unzip_default_instrument.sh $root_dir/usr/local/instruments/unzip_default_instrument.sh
echo "${name}.zip" > $root_dir/usr/local/instruments/default
cp ${tmp_project_path}/${name}.zip $root_dir/usr/local/instruments

# Mount unionfs script for release mode
cp $os_path/scripts/mount_unionfs $root_dir/usr/local/bin/mount_unionfs

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

chmod +x /usr/local/bin/mount_unionfs

cat <<- EOF_CAT > etc/fstab
# /etc/fstab: static file system information.
# <file system> <mount point>   <type>  <options>           <dump>  <pass>
$part2          /               ext4    ro,noatime          0       1
$part1          /boot           vfat    ro,noatime          0       2
tmpfs           /tmp            tmpfs   defaults,noatime    0       0
tmpfs           /var/log        tmpfs   size=1M,noatime     0       0
mount_unionfs   /etc            fuse    defaults,noatime    0       0
mount_unionfs   /var            fuse    defaults,noatime    0       0
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

apt -y install locales

locale-gen en_US.UTF-8
update-locale LANG=en_US.UTF-8

sed -i '/^# deb .* universe$/s/^# //' etc/apt/sources.list

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

pip install werkzeug==0.16.0
# For release mode
apt install -y unionfs-fuse
apt install -y python

systemctl enable unzip-default-instrument
#systemctl enable koheron-server
systemctl enable nginx
timedatectl set-ntp on

sed -i 's/#PermitRootLogin prohibit-password/PermitRootLogin yes/' /etc/ssh/sshd_config

touch etc/udev/rules.d/75-persistent-net-generator.rules

cat <<- EOF_CAT >> etc/network/interfaces
allow-hotplug eth0

# DHCP configuration
iface eth0 inet dhcp

# Static IP
#iface eth0 inet static
#  address 192.168.1.100
#  gateway 192.168.1.1
#  netmask 255.255.255.0
#  network 192.168.1.0
#  broadcast 192.168.1.255
  post-up systemctl start koheron-server-init
EOF_CAT

systemd-machine-id-setup

apt clean
echo root:$passwd | chpasswd
history -c

EOF_CHROOT

# nginx
rm $root_dir/etc/nginx/sites-enabled/default
cp $os_path/config/nginx.conf $root_dir/etc/nginx/nginx.conf
cp $os_path/config/nginx-server-release.conf $root_dir/etc/nginx/sites-enabled/nginx-server.conf
cp $os_path/systemd/nginx.service $root_dir/etc/systemd/system/nginx.service

rm $root_dir/etc/resolv.conf
rm $root_dir/usr/bin/qemu-arm-static

# For release mode
cp -al $root_dir/etc $root_dir/etc_org
mv $root_dir/var $root_dir/var_org
mkdir $root_dir/etc_rw
mkdir $root_dir/var $root_dir/var_rw

#rm $root_dir/etc/resolv.conf
rm $root_dir/usr/bin/qemu-a*
# Unmount file systems

umount $boot_dir $root_dir

rmdir $boot_dir $root_dir

zerofree $root_dev

losetup -d $device
