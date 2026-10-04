#!/bin/bash
set -euo pipefail

# Optional arguments also support offline extraction into a staging directory:
# instruments-directory live-directory loader(auto|overlay|xdevcfg).
instruments_dir=${1:-/usr/local/instruments}
live_dir=${2:-/tmp/live-instrument}
loader=${3:-auto}

case "$loader" in
    auto)
        # Match FpgaManager's preference for xdevcfg when both loaders exist.
        if [ -e /dev/xdevcfg ]; then
            loader=xdevcfg
        elif [ -e /sys/class/fpga_manager/fpga0/flags ]; then
            loader=overlay
        fi
        ;;
    overlay|xdevcfg) ;;
    *) echo "Unknown FPGA loader: $loader" >&2; exit 1 ;;
esac

instrument_zip=$(cat "$instruments_dir/default")

exclude=()
if [ "$loader" = overlay ]; then
    # FPGA Manager uses .bit.bin; leave the original Vivado .bit in the archive.
    exclude=(-x '*.bit')
fi

mkdir -p "$live_dir"
unzip -oq "$instruments_dir/$instrument_zip" -d "$live_dir" "${exclude[@]}"

# Record the extracted identity; the API checks service state before calling it live.
basename "$instrument_zip" .zip > "$live_dir/.instrument-name"
