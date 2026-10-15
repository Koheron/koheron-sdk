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

if [[ -f $uboot_path/drivers/i2c/i2c-cdns.c ]]; then
    fix="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../patches/u-boot/cdns-i2c-final-stop.patch"
    # A configuration change can run this again on the same source tree.
    if ! patch -d "$uboot_path" -p1 -R --dry-run --force --silent --fuzz=0 < "$fix" >/dev/null 2>&1; then
        patch -d "$uboot_path" -p1 --forward --batch --fuzz=0 \
            --no-backup-if-mismatch --reject-file=- < "$fix"
    fi
fi
