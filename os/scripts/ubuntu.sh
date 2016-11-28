device=$1
name=$2
version=$3

koheron_python_branch=v0.11.0

config_dir=os
http_app_dir=tmp/app

boot_dir=/tmp/BOOT
root_dir=/tmp/ROOT

ubuntu_version=16.04.1
root_tar=ubuntu-base-${ubuntu_version}-base-armhf.tar.gz
root_url=http://cdimage.ubuntu.com/ubuntu-base/releases/${ubuntu_version}/release/$root_tar

passwd=changeme
timezone=Europe/Paris

# Create partitions

parted -s $device mklabel msdos
parted -s $device mkpart primary fat16 4MB 16MB
parted -s $device mkpart primary ext4 16MB 100%

boot_dev=/dev/`lsblk -lno NAME $device | sed '2!d'`
root_dev=/dev/`lsblk -lno NAME $device | sed '3!d'`

# Create file systems

mkfs.vfat -v $boot_dev
mkfs.ext4 -F -j $root_dev

# Mount file systems

mkdir -p $boot_dir $root_dir

mount $boot_dev $boot_dir
mount $root_dev $root_dir

# Copy files to the boot file system

cp boot.bin devicetree.dtb uImage $config_dir/uEnv.txt $boot_dir

# Copy Ubuntu Core to the root file system

test -f $root_tar || curl -L $root_url -o $root_tar

tar -zxf $root_tar --directory=$root_dir

# Add missing configuration files and packages

cp /etc/resolv.conf $root_dir/etc/
cp /usr/bin/qemu-arm-static $root_dir/usr/bin/

cp boards/red-pitaya/patches/fw_env.config $root_dir/etc/

cp fw_printenv $root_dir/usr/local/bin/fw_printenv
cp fw_printenv $root_dir/usr/local/bin/fw_setenv

# Add Web app
mkdir $root_dir/usr/local/flask
mkdir $root_dir/usr/local/flask
cp -a $http_app_dir/. $root_dir/usr/local/flask
cp $config_dir/wsgi.py $root_dir/usr/local/flask
unzip -o tmp/static.zip -d $root_dir/var/www

# Add Koheron TCP/Websocket Server
mkdir $root_dir/usr/local/koheron-server
cp tmp/${name}.server.build/kserverd $root_dir/usr/local/koheron-server
cp $config_dir/koheron-server.conf $root_dir/usr/local/koheron-server
cp tmp/${name}.koheron-server/VERSION $root_dir/usr/local/koheron-server
cp $config_dir/systemd/koheron-server.service $root_dir/etc/systemd/system/koheron-server.service
cp $config_dir/systemd/koheron-server-init.service $root_dir/etc/systemd/system/koheron-server-init.service

# uwsgi
mkdir $root_dir/etc/flask-uwsgi
cp $config_dir/flask-uwsgi.ini $root_dir/etc/flask-uwsgi/flask-uwsgi.ini
cp $config_dir/systemd/uwsgi.service $root_dir/etc/systemd/system/uwsgi.service

# Add zip
mkdir $root_dir/usr/local/instruments
cp tmp/*-${version}.zip $root_dir/usr/local/instruments
echo "last_deployed: /usr/local/instruments/${name}-${version}.zip" > $root_dir/usr/local/instruments/.instruments
# Copy all available instruments in backup directory
mkdir $root_dir/usr/local/instruments/backup
cp tmp/*-${version}.zip $root_dir/usr/local/instruments/backup

chroot $root_dir <<- EOF_CHROOT
export LANG=C
export LC_ALL=C

# Add /usr/local/koheron-server to the environment PATH
cat <<- EOF_CAT > etc/environment
PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games:/usr/local/games:/usr/local/koheron-server"
EOF_CAT

cat <<- EOF_CAT > etc/koheron_sdk_version
$version
EOF_CAT

cat <<- EOF_CAT > etc/apt/apt.conf.d/99norecommends
APT::Install-Recommends "0";
APT::Install-Suggests "0";
EOF_CAT

cat <<- EOF_CAT > etc/fstab
# /etc/fstab: static file system information.
# <file system> <mount point>   <type>  <options>           <dump>  <pass>
/dev/mmcblk0p2  /               ext4    errors=remount-ro   0       1
/dev/mmcblk0p1  /boot           vfat    defaults            0       2
tmpfs           /tmp            tmpfs   defaults            0       0
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

apt-get -y install locales

locale-gen en_US.UTF-8
update-locale LANG=en_US.UTF-8

sed -i '/^# deb .* universe$/s/^# //' etc/apt/sources.list

apt-get update
apt-get -y upgrade

echo $timezone > etc/timezone
dpkg-reconfigure --frontend=noninteractive tzdata

apt-get -y install openssh-server ntp usbutils psmisc lsof \
  parted curl less vim iw ntfs-3g \
  bash-completion unzip

apt-get install -y udev net-tools netbase ifupdown lsb-base
apt-get install -y ntpdate sudo

apt-get install -y nginx
apt-get install -y build-essential python-dev
apt-get install -y python-numpy
apt-get install -y python-pip python-setuptools python-all-dev python-wheel

pip install --upgrade pip
pip install https://github.com/Koheron/koheron-python/zipball/$koheron_python_branch
pip install flask
pip install jinja2
pip install urllib3
pip install pyyaml
pip install uwsgi

systemctl enable uwsgi
systemctl enable koheron-server
systemctl enable nginx

sed -i 's/^PermitRootLogin.*/PermitRootLogin yes/' etc/ssh/sshd_config

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
  post-up ntpdate -u ntp.u-psud.fr
  post-up systemctl start koheron-server-init
EOF_CAT

apt-get clean
echo root:$passwd | chpasswd
service ntp stop
history -c

EOF_CHROOT

# nginx
rm $root_dir/etc/nginx/sites-enabled/default
cp $config_dir/nginx.conf $root_dir/etc/nginx/nginx.conf
cp $config_dir/flask-uwsgi $root_dir/etc/nginx/sites-enabled/flask-uwsgi
cp $config_dir/systemd/nginx.service $root_dir/etc/systemd/system/nginx.service

rm $root_dir/etc/resolv.conf
rm $root_dir/usr/bin/qemu-arm-static

# Unmount file systems

umount $boot_dir $root_dir

rmdir $boot_dir $root_dir
