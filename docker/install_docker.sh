#!/usr/bin/env bash
set -e

ME="${SUDO_USER:-$(id -un)}"

# Repo setup
sudo apt-get update
sudo apt-get install -y ca-certificates curl gnupg
sudo install -m 0755 -d /etc/apt/keyrings
curl -fsSL https://download.docker.com/linux/ubuntu/gpg \
  | sudo gpg --dearmor -o /etc/apt/keyrings/docker.gpg
sudo chmod a+r /etc/apt/keyrings/docker.gpg
echo "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.gpg] \
https://download.docker.com/linux/ubuntu $(. /etc/os-release; echo $UBUNTU_CODENAME) stable" \
| sudo tee /etc/apt/sources.list.d/docker.list > /dev/null

# Install
sudo apt-get update
sudo apt-get install -y docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin

# Service
sudo systemctl enable --now docker

# Group & perms
getent group docker >/dev/null || sudo groupadd docker
sudo usermod -aG docker "$ME"
[ -S /var/run/docker.sock ] && { sudo chgrp docker /var/run/docker.sock; sudo chmod 660 /var/run/docker.sock; }

# Immediate test without logout
sudo -u "$ME" sg docker -c 'docker run --rm hello-world && echo "Docker OK for $USER"'
