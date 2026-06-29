#!/bin/bash
set -euo pipefail

instrument_zip=$(cat /usr/local/instruments/default)

mkdir -p /tmp/live-instrument
unzip -o "/usr/local/instruments/${instrument_zip}" -d /tmp/live-instrument
