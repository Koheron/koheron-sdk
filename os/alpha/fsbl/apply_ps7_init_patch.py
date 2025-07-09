#!/usr/bin/env python3

import sys
import os
import shutil

# Modify the order in which two pins connected to the USB PHY are initialised by the FSBL
# https://github.com/Koheron/koheron-sdk/issues/618

PATCH_BLOCK = [
    "    // .. TRI_ENABLE = 0\n",
    "    // .. ==> 0XF8000778[0:0] = 0x00000000U\n",
    "    // ..     ==> MASK : 0x00000001U    VAL : 0x00000000U\n",
    "    // .. L0_SEL = 0\n",
    "    // .. ==> 0XF8000778[1:1] = 0x00000000U\n",
    "    // ..     ==> MASK : 0x00000002U    VAL : 0x00000000U\n",
    "    // .. L1_SEL = 1\n",
    "    // .. ==> 0XF8000778[2:2] = 0x00000001U\n",
    "    // ..     ==> MASK : 0x00000004U    VAL : 0x00000004U\n",
    "    // .. L2_SEL = 0\n",
    "    // .. ==> 0XF8000778[4:3] = 0x00000000U\n",
    "    // ..     ==> MASK : 0x00000018U    VAL : 0x00000000U\n",
    "    // .. L3_SEL = 0\n",
    "    // .. ==> 0XF8000778[7:5] = 0x00000000U\n",
    "    // ..     ==> MASK : 0x000000E0U    VAL : 0x00000000U\n",
    "    // .. Speed = 0\n",
    "    // .. ==> 0XF8000778[8:8] = 0x00000000U\n",
    "    // ..     ==> MASK : 0x00000100U    VAL : 0x00000000U\n",
    "    // .. IO_Type = 1\n",
    "    // .. ==> 0XF8000778[11:9] = 0x00000001U\n",
    "    // ..     ==> MASK : 0x00000E00U    VAL : 0x00000200U\n",
    "    // .. PULLUP = 1\n",
    "    // .. ==> 0XF8000778[12:12] = 0x00000001U\n",
    "    // ..     ==> MASK : 0x00001000U    VAL : 0x00001000U\n",
    "    // .. DisableRcvr = 0\n",
    "    // .. ==> 0XF8000778[13:13] = 0x00000000U\n",
    "    // ..     ==> MASK : 0x00002000U    VAL : 0x00000000U\n",
    "    // .. \n",
    "    EMIT_MASKWRITE(0XF8000778, 0x00003FFFU ,0x00001204U),\n"
]

def patch_ps7_init(path):
    shutil.copy2(path, path + ".orig")
    with open(path, "r", encoding="utf-8") as f:
        lines = f.readlines()

    result = []
    i = 0
    while i < len(lines):
        # detect 0774 write followed by the patch block (heuristic)
        if lines[i].strip() == "EMIT_MASKWRITE(0XF8000774, 0x00003FFFU ,0x00001205U)," and \
           i + len(PATCH_BLOCK) < len(lines) and lines[i+1].strip() == "// .. TRI_ENABLE = 0":
            result.append(lines[i])  # keep 0774 line
            i += len(PATCH_BLOCK) + 1  # skip the patch block
            continue

        result.append(lines[i])
        i += 1

    # insert the patch block after every 0x079C
    final = []
    for line in result:
        final.append(line)
        if line.strip() == "EMIT_MASKWRITE(0XF800079C, 0x00003FFFU ,0x00001204U),":
            final.extend(PATCH_BLOCK)

    with open(path, "w", encoding="utf-8") as f:
        f.writelines(final)

    print("Patch applied.")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: apply_ps7_init_patch.py <fsbl_build_dir>")
        sys.exit(1)

    patch_ps7_init(os.path.join(sys.argv[1], "ps7_init.c"))