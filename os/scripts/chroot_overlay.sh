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
systemctl enable uwsgi || true
systemctl enable uwsgi.socket || true
systemctl enable grow-rootfs-once.service || true
systemctl enable unzip-default-instrument || true
systemctl enable koheron-server || true
systemctl enable koheron-server-init || true
systemctl enable nginx || true
systemctl enable systemd-networkd.service
systemctl enable systemd-resolved.service
systemctl enable systemd-timesyncd.service
exit 0
