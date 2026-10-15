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

# Xilinx's Cadence driver keeps HOLD enabled for every message in a combined
# transfer. The final EEPROM read then never emits STOP and times out, losing
# the Red Pitaya's factory MAC. Recompute the flag after each message/retry.
python3 - "$uboot_path/drivers/i2c/i2c-cdns.c" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1])
if path.exists():
    source = path.read_text()
    before = '\t\tdebug("i2c_xfer: chip=0x%x, len=0x%x\\n", msg->addr, msg->len);'
    after = ('\t\t/* Release the bus after the final message, including retries. */\n'
             '\t\ti2c_bus->hold_flag = nmsgs > 1;\n' + before)
    if after not in source:
        if source.count(before) != 1:
            sys.exit(f'{path}: cannot locate Cadence I2C transfer loop')
        path.write_text(source.replace(before, after))
PY
