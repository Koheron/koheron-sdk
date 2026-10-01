#!/bin/bash
set -euo pipefail

instrument_zip=$(cat /usr/local/instruments/default)

mkdir -p /tmp/live-instrument
unzip -o "/usr/local/instruments/${instrument_zip}" -d /tmp/live-instrument

# Record the extracted identity; the API checks service state before calling it live.
basename "$instrument_zip" .zip > /tmp/live-instrument/.instrument-name
