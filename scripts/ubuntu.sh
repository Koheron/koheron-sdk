device=$1
name=$2
version=$3

config_dir=os
http_app_dir=tmp/app

boot_dir=/tmp/BOOT
root_dir=/tmp/ROOT

ubuntu_version=16.04.1
root_tar=ubuntu-base-${ubuntu_version}-base-armhf.tar.gz
root_url=http://cdimage.ubuntu.com/ubuntu-base/releases/${ubuntu_version}/release/$root_tar

hostapd_url=https://googledrive.com/host/0B-t5klOOymMNfmJ0bFQzTVNXQ3RtWm5SQ2NGTE1hRUlTd3V2emdSNzN6d0pYamNILW83Wmc/rtl8192cu/hostapd-armhf

passwd=changeme
timezone=Europe/Brussels

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
cp tmp/${name}.koheron-server/tmp/kserverd $root_dir/usr/local/koheron-server
cp $config_dir/koheron-server.conf $root_dir/usr/local/koheron-server
cp tmp/${name}.koheron-server/VERSION $root_dir/usr/local/koheron-server
cp $config_dir/koheron-server.service $root_dir/etc/systemd/system/koheron-server.service
cp $config_dir/koheron-server-init.service $root_dir/etc/systemd/system/koheron-server-init.service

# uwsgi
mkdir $root_dir/etc/flask-uwsgi
cp $config_dir/flask-uwsgi.ini $root_dir/etc/flask-uwsgi/flask-uwsgi.ini
cp $config_dir/uwsgi.service $root_dir/etc/systemd/system/uwsgi.service

# Add zip
mkdir $root_dir/usr/local/instruments
cp tmp/*-${version}.zip $root_dir/usr/local/instruments
echo "last_deployed: /usr/local/instruments/${name}-${version}.zip" > $root_dir/usr/local/instruments/.instruments
# Copy all available instruments in backup directory
mkdir $root_dir/usr/local/instruments/backup
cp tmp/*-${version}.zip $root_dir/usr/local/instruments/backup

curl -L $hostapd_url -o $root_dir/usr/local/sbin/hostapd
chmod +x $root_dir/usr/local/sbin/hostapd

chroot $root_dir <<- EOF_CHROOT
export LANG=C
export LC_ALL=C

# Add /usr/local/koheron-server to the environment PATH
cat <<- EOF_CAT > etc/environment
PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games:/usr/local/games:/usr/local/koheron-server"
EOF_CAT

cat <<- EOF_CAT > etc/zynq_sdk_version
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
EOF_CAT

cat <<- EOF_CAT >> etc/securetty

# Serial Console for Xilinx Zynq-7000
ttyPS0
EOF_CAT

#sed 's/tty1/ttyPS0/g; s/38400/115200/' etc/init/tty1.conf > etc/init/ttyPS0.conf

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
  parted curl less vim iw  ntfs-3g gdb  \
  bash-completion unzip rsync 

# Packages needed for Docker
apt-get install -y lxc cgroup-lite docker.io
cat <<- EOF_CAT >> etc/default/lxc-net
USE_LXC_BRIDGE="false"
EOF_CAT

#apt-get install -y wpasupplicant linux-firmware usbutils ca-certificates

apt-get install -y udev net-tools netbase ifupdown network-manager lsb-base
apt-get install -y ntpdate sudo

apt-get install -y nginx
apt-get install -y build-essential python-dev
apt-get install -y python-numpy
apt-get install -y python-pip python-setuptools python-all-dev python-wheel

pip install --upgrade pip
pip install https://github.com/Koheron/koheron-python/zipball/cli
pip install flask
pip install jinja2
pip install urllib3
pip install pyyaml
pip install uwsgi

systemctl enable uwsgi
systemctl enable koheron-server
systemctl enable nginx

sed -i 's/^PermitRootLogin.*/PermitRootLogin yes/' etc/ssh/sshd_config

apt-get -y install hostapd isc-dhcp-server iptables

touch etc/udev/rules.d/75-persistent-net-generator.rules

cat <<- EOF_CAT > etc/rc.local
#!/bin/sh -e
# rc.local
#/usr/local/koheron-server/kserverd -c /usr/local/koheron-server/koheron-server.conf
exit 0
EOF_CAT

cat <<- EOF_CAT >> etc/network/interfaces
allow-hotplug eth0

# DHCP configuration
iface eth0 inet dhcp

# Static IP
#iface eth0 inet static
#  address 192.168.1.100
#  gateway 192.168.1.0
#  netmask 255.255.255.0
#  network 192.168.1.0
#  broadcast 192.168.1.255
  post-up ntpdate -u ntp.u-psud.fr
  post-up systemctl start koheron-server-init

allow-hotplug wlan0
iface wlan0 inet static
  address 192.168.42.1
  netmask 255.255.255.0
  post-up service hostapd restart
  post-up service isc-dhcp-server restart
  post-up iptables-restore < /etc/iptables.ipv4.nat
  post-up ntpdate -u ntp.u-psud.fr
  post-up systemctl start koheron-server-init
  pre-down iptables-restore < /etc/iptables.ipv4.nonat
  pre-down service isc-dhcp-server stop
  pre-down service hostapd stop
EOF_CAT

cat <<- EOF_CAT > etc/hostapd/hostapd.conf
interface=wlan0
ssid=koheron
driver=nl80211
hw_mode=g
channel=6
macaddr_acl=0
auth_algs=1
ignore_broadcast_ssid=0
wpa=2
wpa_passphrase=koheron
wpa_key_mgmt=WPA-PSK
wpa_pairwise=TKIP
rsn_pairwise=CCMP
EOF_CAT

cat <<- EOF_CAT > etc/default/hostapd
DAEMON_CONF=/etc/hostapd/hostapd.conf

if [ "\\\$1" = "start" ]
then
  iw wlan0 info > /dev/null 2>&1
  if [ \\\$? -eq 0 ]
  then
    sed -i '/^driver/s/=.*/=nl80211/' /etc/hostapd/hostapd.conf
    DAEMON_SBIN=/usr/sbin/hostapd
  else
    sed -i '/^driver/s/=.*/=rtl871xdrv/' /etc/hostapd/hostapd.conf
    DAEMON_SBIN=/usr/local/sbin/hostapd
  fi
  echo \\\$DAEMON_SBIN > /run/hostapd.which
elif [ "\\\$1" = "stop" ]
then
  DAEMON_SBIN=\\\$(cat /run/hostapd.which)
fi
EOF_CAT

cat <<- EOF_CAT > etc/dhcp/dhcpd.conf
ddns-update-style none;
default-lease-time 600;
max-lease-time 7200;
authoritative;
log-facility local7;
subnet 192.168.42.0 netmask 255.255.255.0 {
  range 192.168.42.10 192.168.42.50;
  option broadcast-address 192.168.42.255;
  option routers 192.168.42.1;
  default-lease-time 600;
  max-lease-time 7200;
  option domain-name "local";
  option domain-name-servers 8.8.8.8, 8.8.4.4;
}
EOF_CAT

sed -i '/^#net.ipv4.ip_forward=1$/s/^#//' etc/sysctl.conf

cat <<- EOF_CAT > etc/iptables.ipv4.nat
*nat
:PREROUTING ACCEPT [0:0]
:INPUT ACCEPT [0:0]
:OUTPUT ACCEPT [0:0]
:POSTROUTING ACCEPT [0:0]
-A POSTROUTING -o eth0 -j MASQUERADE
COMMIT
*mangle
:PREROUTING ACCEPT [0:0]
:INPUT ACCEPT [0:0]
:FORWARD ACCEPT [0:0]
:OUTPUT ACCEPT [0:0]
:POSTROUTING ACCEPT [0:0]
COMMIT
*filter
:INPUT ACCEPT [0:0]
:FORWARD ACCEPT [0:0]
:OUTPUT ACCEPT [0:0]
-A FORWARD -i eth0 -o wlan0 -m state --state RELATED,ESTABLISHED -j ACCEPT
-A FORWARD -i wlan0 -o eth0 -j ACCEPT
COMMIT
EOF_CAT

cat <<- EOF_CAT > etc/iptables.ipv4.nonat
*nat
:PREROUTING ACCEPT [0:0]
:INPUT ACCEPT [0:0]
:OUTPUT ACCEPT [0:0]
:POSTROUTING ACCEPT [0:0]
COMMIT
*mangle
:PREROUTING ACCEPT [0:0]
:INPUT ACCEPT [0:0]
:FORWARD ACCEPT [0:0]
:OUTPUT ACCEPT [0:0]
:POSTROUTING ACCEPT [0:0]
COMMIT
*filter
:INPUT ACCEPT [0:0]
:FORWARD ACCEPT [0:0]
:OUTPUT ACCEPT [0:0]
COMMIT
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
cp $config_dir/nginx.service $root_dir/etc/systemd/system/nginx.service

rm $root_dir/etc/resolv.conf
rm $root_dir/usr/bin/qemu-arm-static

# Unmount file systems

umount $boot_dir $root_dir

rmdir $boot_dir $root_dir
