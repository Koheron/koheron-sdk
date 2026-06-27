#!/usr/bin/env bash
set -euo pipefail

# Make apt/dpkg non-interactive and set a sane timezone if missing
export DEBIAN_FRONTEND=${DEBIAN_FRONTEND:-noninteractive}
export TZ=${TZ:-UTC}

ME="${SUDO_USER:-$(id -un)}"

# Repo setup (idempotent)
sudo apt-get update -y -q
sudo apt-get install -y -q --no-install-recommends ca-certificates curl gnupg lsb-release

sudo install -d -m 0755 /etc/apt/keyrings
# dearmor will overwrite without prompting thanks to --batch --yes
curl -fsSL https://download.docker.com/linux/ubuntu/gpg \
  | sudo gpg --dearmor --batch --yes -o /etc/apt/keyrings/docker.gpg
sudo chmod a+r /etc/apt/keyrings/docker.gpg

# Write/refresh the Docker apt source (overwrite is fine; no prompt)
CODENAME="$(. /etc/os-release; echo "${UBUNTU_CODENAME}")"
echo "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.gpg] https://download.docker.com/linux/ubuntu ${CODENAME} stable" \
  | sudo tee /etc/apt/sources.list.d/docker.list >/dev/null

# Install Docker Engine + plugins
sudo apt-get update -y -q
sudo apt-get install -y -q \
  docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin \
  -o Dpkg::Options::=--force-confdef -o Dpkg::Options::=--force-confnew

# Enable service (don't fail on systems without systemd)
if command -v systemctl >/dev/null 2>&1; then
  sudo systemctl enable --now docker || true
fi

# Group & permissions
getent group docker >/dev/null || sudo groupadd docker
sudo usermod -aG docker "$ME"

# If dockerd is up, fix socket perms for immediate use
if [ -S /var/run/docker.sock ]; then
  sudo chgrp docker /var/run/docker.sock || true
  sudo chmod 660 /var/run/docker.sock || true
fi

# Immediate test without re-login (run in docker group)
if command -v sg >/dev/null 2>&1; then
  sudo -u "$ME" sg docker -c 'docker run --rm hello-world && echo "Docker OK for $USER"'
else
  echo "sg not available; you may need to log out/in to use Docker without sudo."
fi
