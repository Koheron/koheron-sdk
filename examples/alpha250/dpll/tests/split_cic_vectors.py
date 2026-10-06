"""Exact integer reference for six-stage CIC at the TOTAL rate, before FIR."""
from collections import deque
from pathlib import Path
import sys

out = Path(sys.argv[1])
out.mkdir(parents=True, exist_ok=True)
for rate in (4, 6, 8, 10, 14, 20, 32, 126, 128, 130, 8190, 8192):
    sums = [0] * 6
    histories = [deque([0] * rate) for _ in range(6)]
    delayed = deque([0] * 15)  # Five fast stages + five /2 input stages.
    shift = (rate**6 - 1).bit_length() - 8
    samples = []
    for index in range(rate * 16):
        raw = ((index * 0x9E3779B9) ^ (index << 7) ^ (index >> 3)) & 0xFFFFFFFF
        if index >= 1024:
            raw = 0x7FFFFFFF if index < 8 * rate else 0x80000000
        value = raw - (1 << 32) if raw & (1 << 31) else raw
        for stage in range(6):
            sums[stage] += value - histories[stage].popleft()
            histories[stage].append(value)
            value = sums[stage]
        delayed.append(value)
        value = delayed.popleft()
        if index % rate == rate - 1:
            samples.append((value >> shift) & ((1 << 40) - 1))
    (out / f"reference_{rate}.mem").write_text("".join(f"{v:010x}\n" for v in samples))
