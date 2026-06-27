#!/usr/bin/env bash
set -euo pipefail

export DEBIAN_FRONTEND=${DEBIAN_FRONTEND:-noninteractive}

apt-get update
apt-get install -y curl rsync python3-venv qemu-user-static