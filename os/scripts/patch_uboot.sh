#!/usr/bin/env bash
set -euo pipefail
uboot_path=${1:?Usage: patch_uboot.sh <u-boot-source>}

# SPL needs binary zero padding to a four-byte boundary. conv=block is a
# text-record conversion with no effect when cbs is unset; newer coreutils
# rejects that combination. Keep only the binary padding conversion.
for makefile in scripts/Makefile.spl scripts/Makefile.xpl; do
    if [[ -f $uboot_path/$makefile ]]; then
        sed -i 's/conv=block,sync bs=4/conv=sync bs=4/g' "$uboot_path/$makefile"
    fi
done
