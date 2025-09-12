#!/usr/bin/env bash
set -euo pipefail

# Optional: set this to add your non-root user to the docker group.
# If empty, the script tries SUDO_USER; if still empty, it skips adding.
TARGET_USER="${TARGET_USER:-${SUDO_USER:-}}"

# 1) Prereqs
apt-get update -y
apt-get install -y ca-certificates curl

# 2) Keyring
install -m 0755 -d /etc/apt/keyrings
curl -fsSL https://download.docker.com/linux/ubuntu/gpg \
  -o /etc/apt/keyrings/docker.asc
chmod a+r /etc/apt/keyrings/docker.asc

# 3) Repo
UBUNTU_CODENAME="$(. /etc/os-release && echo "$VERSION_CODENAME")"
ARCH="$(dpkg --print-architecture)"
echo "deb [arch=${ARCH} signed-by=/etc/apt/keyrings/docker.asc] https://download.docker.com/linux/ubuntu ${UBUNTU_CODENAME} stable" \
  > /etc/apt/sources.list.d/docker.list

apt-get update -y

# 4) Install Docker Engine + plugins
apt-get install -y \
  docker-ce docker-ce-cli containerd.io \
  docker-buildx-plugin docker-compose-plugin

# 5) Enable/Start (when systemd is available)
if command -v systemctl >/dev/null 2>&1 && [ "$(ps -p 1 -o comm=)" = "systemd" ]; then
  systemctl enable --now docker || true
else
  # Fallback for non-systemd/chroot/container
  service docker start 2>/dev/null || true
fi

# 6) Add user to docker group (if we have a non-root user)
if [ -n "${TARGET_USER}" ] && id -u "${TARGET_USER}" >/dev/null 2>&1; then
  usermod -aG docker "${TARGET_USER}"
  echo "Added ${TARGET_USER} to the 'docker' group. They must log out/in for it to take effect."
else
  echo "Skipping docker group add (no TARGET_USER/SUDO_USER set or user not found)."
fi

echo "Docker installation complete."