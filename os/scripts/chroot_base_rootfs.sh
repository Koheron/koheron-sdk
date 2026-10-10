#!/usr/bin/env bash
set -Eeuo pipefail

# Inputs via env
TIMEZONE="${TIMEZONE:-Europe/Paris}"
PASSWD="${PASSWD:-changeme}"

export DEBIAN_FRONTEND=noninteractive
export LANG=C
export LC_ALL=C

# Package installation must not start services in the build chroot.
policy_created=0
if [ ! -e /usr/sbin/policy-rc.d ]; then
  printf '#!/bin/sh\nexit 101\n' > /usr/sbin/policy-rc.d
  chmod 0755 /usr/sbin/policy-rc.d
  policy_created=1
fi
cleanup_policy() {
  if [ "$policy_created" -eq 1 ]; then
    rm -f /usr/sbin/policy-rc.d
  fi
}
trap cleanup_policy EXIT

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
path-include=/usr/share/doc/*/copyright
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

# Use the extracted rootfs release, including when UBUNTU_VERSION is overridden.
source /etc/os-release
if [[ ${ID:-} != ubuntu || ! ${VERSION_CODENAME:-} =~ ^[a-z]+$ ]]; then
  echo 'Expected an Ubuntu rootfs with a valid VERSION_CODENAME' >&2
  exit 1
fi
cat > /etc/apt/sources.list <<EOF_SOURCES
deb http://ports.ubuntu.com/ubuntu-ports ${VERSION_CODENAME} main universe
deb http://ports.ubuntu.com/ubuntu-ports ${VERSION_CODENAME}-updates main universe
deb http://ports.ubuntu.com/ubuntu-ports ${VERSION_CODENAME}-security main universe
EOF_SOURCES
rm -f /etc/apt/sources.list.d/ubuntu.sources

# /dev is bind-mounted from the builder; never replace its device nodes.
test -c /dev/null

# Minimal modules file for dpkg triggers’ sanity
install -D -m0644 /dev/null /etc/modules

# Do not build from stale indexes when a repository refresh only partly succeeds.
apt-get update --error-on=any
apt-get -yq -o Dpkg::Use-Pty=0 install --no-install-recommends eatmydata tzdata

# systemd-related system users (tmpfiles expects them)
getent group systemd-journal >/dev/null 2>&1 || groupadd --system systemd-journal
id -u systemd-network >/dev/null 2>&1 || useradd --system --home /run/systemd/network --no-create-home --user-group systemd-network

# Timezone
echo "$TIMEZONE" > /etc/timezone
dpkg-reconfigure --frontend=noninteractive tzdata

# Core packages (no recommends)
eatmydata apt-get -yq install -o Dpkg::Use-Pty=0 --no-install-recommends \
  systemd systemd-sysv systemd-timesyncd systemd-resolved \
  openssh-server usbutils psmisc lsof parted curl less nano iw \
  fdisk e2fsprogs bash-completion udev net-tools netbase \
  lsb-base sudo rsync kmod nginx \
  libmicrohttpd12t64 libzip5 libjson-c5 libunistring5 libstdc++6 \
  iproute2

# Also clean an Ubuntu Base archive or cache with obsolete runtime packages.
# cloud-guest-utils depended on Python; first-boot growth now uses sfdisk.
# Native extraction replaces unzip at boot.
mapfile -t obsolete_packages < <(
  dpkg-query -W -f='${binary:Package}\t${db:Status-Abbrev}\n' \
    'python*' 'libpython*' 'uwsgi*' cloud-guest-utils unzip 2>/dev/null |
    awk '$2 ~ /^ii/ { print $1 }'
)
if [ "${#obsolete_packages[@]}" -gt 0 ]; then
  apt-get -yq -o Dpkg::Use-Pty=0 purge --auto-remove "${obsolete_packages[@]}"
fi
if command -v python3 >/dev/null || command -v python >/dev/null || command -v uwsgi >/dev/null; then
  echo 'Unexpected Python/uWSGI interpreter in the board rootfs' >&2
  exit 1
fi

# glibc provides C.UTF-8 without the locales package or a generated archive.
# Set it after installing systemd, which migrates /etc/default/locale.
printf 'LANG=C.UTF-8\n' > /etc/default/locale

# eatmydata is only needed while building; keep it out of the shipped rootfs.
apt-get -yq -o Dpkg::Use-Pty=0 purge eatmydata libeatmydata1

# Clean & hygiene
apt-get clean
rm -rf /var/lib/apt/lists/*
printf 'root:%s\n' "$PASSWD" | chpasswd
rm -f /root/.bash_history /root/.ash_history /root/.python_history /root/.lesshst || true
