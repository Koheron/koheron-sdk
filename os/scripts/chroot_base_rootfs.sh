#!/usr/bin/env bash
set -Eeuo pipefail

# Inputs via env
TIMEZONE="${TIMEZONE:-Europe/Paris}"
PASSWD="${PASSWD:-changeme}"

export DEBIAN_FRONTEND=noninteractive
export LANG=C
export LC_ALL=C

# PATH for login shells
cat >/etc/environment <<'EOF'
PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/local/koheron-server"
EOF

# Faster/leaner apt & dpkg
cat > /etc/dpkg/dpkg.cfg.d/02_nofsync <<'EOF_DPKG_IO'
force-unsafe-io
EOF_DPKG_IO
cat > /etc/dpkg/dpkg.cfg.d/01_nodoc <<'EOF_NODOC'
path-exclude=/usr/share/doc/*
path-exclude=/usr/share/man/*
path-exclude=/usr/share/info/*
path-exclude=/usr/share/locale/*
path-include=/usr/share/locale/en/*
path-include=/usr/share/locale/en_US/*
EOF_NODOC
cat > /etc/apt/apt.conf.d/99nolangs <<'EOF_NOLANGS'
Acquire::Languages "none";
EOF_NOLANGS
cat > /etc/apt/apt.conf.d/99norecommends <<'EOF_APT'
APT::Install-Recommends "0";
APT::Install-Suggests "0";
EOF_APT
cat > /etc/apt/apt.conf.d/99conf <<'EOF_CONF'
Dpkg::Options { "--force-confdef"; "--force-confold"; };
Acquire::Retries "3";
EOF_CONF

# Basic identity
printf '%s\n' 'ttyPS0' >> /etc/securetty
echo koheron > /etc/hostname
cat >> /etc/hosts <<'EOF_HOSTS'
127.0.0.1    localhost.localdomain localhost
127.0.1.1    koheron
EOF_HOSTS

# Ubuntu Noble on ARM sources
cat > /etc/apt/sources.list <<'EOF_SOURCES'
deb http://ports.ubuntu.com/ubuntu-ports noble main universe
deb http://ports.ubuntu.com/ubuntu-ports noble-updates main universe
deb http://ports.ubuntu.com/ubuntu-ports noble-security main universe
# deb http://ports.ubuntu.com/ubuntu-ports noble-backports main universe
EOF_SOURCES
rm -f /etc/apt/sources.list.d/ubuntu.sources

# /dev/null safety
rm -f /dev/null && mknod /dev/null c 1 3 && chmod 666 /dev/null

# Minimal modules file for dpkg triggers’ sanity
install -D -m0644 /dev/null /etc/modules

apt-get update
apt-get -yq -o Dpkg::Use-Pty=0 install --no-install-recommends locales eatmydata tzdata

# systemd-related system users (tmpfiles expects them)
getent group systemd-journal >/dev/null 2>&1 || groupadd --system systemd-journal
id -u systemd-network >/dev/null 2>&1 || useradd --system --home /run/systemd/network --no-create-home --user-group systemd-network

# Locale & timezone
locale-gen en_US.UTF-8
update-locale LANG=en_US.UTF-8
echo "$TIMEZONE" > /etc/timezone
dpkg-reconfigure --frontend=noninteractive tzdata

# Core packages (no recommends)
eatmydata apt-get -yq install -o Dpkg::Use-Pty=0 --no-install-recommends \
  systemd systemd-sysv systemd-timesyncd systemd-resolved \
  openssh-server usbutils psmisc lsof parted curl less nano iw \
  cloud-guest-utils e2fsprogs bash-completion unzip udev net-tools netbase \
  lsb-base sudo rsync kmod nginx \
  python3-flask uwsgi-core uwsgi-plugin-python3 python3-simplejson \
  iproute2

# Networkd config (end0 via DHCP)
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

# resolv.conf -> systemd-resolved stub
rm -f /etc/resolv.conf
ln -s ../run/systemd/resolve/stub-resolv.conf /etc/resolv.conf

# SSH policy (keep like your original)
sed -i 's/#\?PermitRootLogin.*/PermitRootLogin yes/' /etc/ssh/sshd_config

# Clean & hygiene
eatmydata apt-get clean
rm -rf /var/lib/apt/lists/*
echo "root:${PASSWD}" | chpasswd
rm -f /root/.bash_history /root/.ash_history /root/.python_history /root/.lesshst || true
