#!/usr/bin/env bash
set -Eeuo pipefail

# Optional envs for overlay-time tweaks
export DEBIAN_FRONTEND=noninteractive
export LANG=C
export LC_ALL=C

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

cat >/etc/systemd/network/10-end1.network <<'EOF'
[Match]
Name=end1
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


# ---- Enable services (create symlinks even in chroot) ----
# A reused rootfs can still have the retired runtime's enablement links.
rm -f /etc/systemd/system/{basic,multi-user}.target.wants/uwsgi.service \
      /etc/systemd/system/sockets.target.wants/uwsgi.socket \
      /etc/systemd/system/uwsgi.service /etc/systemd/system/uwsgi.socket
rm -rf /etc/uwsgi /usr/local/api/app
rm -f /usr/local/api/wsgi.py /usr/local/koheron-server/koheron-server-init.py
# Recreate these links: older images enabled the services under basic.target.
# Merely enabling them again leaves those old dependencies behind.
systemctl reenable koheron-api
systemctl enable koheron-api.socket
systemctl enable grow-rootfs-once.service
systemctl reenable unzip-default-instrument
systemctl reenable koheron-server
systemctl enable koheron-server-init
systemctl reenable nginx
systemctl enable systemd-networkd.service
systemctl enable systemd-resolved.service
systemctl enable systemd-timesyncd.service
exit 0
