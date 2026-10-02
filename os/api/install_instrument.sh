#!/bin/bash
set -euo pipefail

NAME=$1
LIVE_DIRNAME=$2

exec /usr/bin/python3 "$(dirname "$0")/install_instrument.py" \
    "/usr/local/instruments/${NAME}.zip" "$LIVE_DIRNAME"
