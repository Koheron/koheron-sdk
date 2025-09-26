#!/usr/bin/env bash
set -Eeuo pipefail

# Optional envs for overlay-time tweaks
export DEBIAN_FRONTEND=noninteractive
export LANG=C
export LC_ALL=C

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
