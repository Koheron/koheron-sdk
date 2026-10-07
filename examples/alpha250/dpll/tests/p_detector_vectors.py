"""Projection vectors over phase, amplitude, sign boundaries and saturation."""
import math
import random
import sys
from pathlib import Path

scale = 8192 / math.pi
fraction = 1 << 18
rows = []
for angle in range(-180, 180, 15):
    for amplitude in (64, 4096, 30000):
        i0 = round(amplitude * math.cos(math.radians(angle)))
        q0 = round(amplitude * math.sin(math.radians(angle)))
        norm = i0*i0 + q0*q0
        cx = round(-q0 * scale * fraction / norm)
        cy = round(i0 * scale * fraction / norm)
        for residual in range(-20, 21):
            for ratio in (.25, .5, 1, 1.5):
                phi = math.atan2(q0, i0) + math.radians(residual)
                amp = math.sqrt(norm) * ratio
                i = max(-32768, min(32767, round(amp * math.cos(phi))))
                q = max(-32768, min(32767, round(amp * math.sin(phi))))
                # Independent floating-point phase/amplitude representation.
                d = math.atan2(q, i) - math.atan2(q0, i0)
                projected = scale * math.hypot(i, q) / math.sqrt(norm) * math.sin(d)
                golden = max(-65536, min(65535, math.floor(projected)))
                y = i*cx + q*cy
                x = i*cy - q*cx
                valid = x > 341782528 and 4*abs(y) < x
                rows.append((cx, cy, i, q, golden, int(valid), 1))

rng = random.Random(31745)
edges = (-(1 << 24), -1, 0, 1, (1 << 24) - 1)
samples = (-32768, 0, 32767)
for n in range(5000):
    if n < 225:
        cx, cy = edges[n // 45], edges[(n // 9) % 5]
        i, q = samples[(n // 3) % 3], samples[n % 3]
    else:
        cx, cy = [rng.randrange(-(1 << 24), 1 << 24) for _ in range(2)]
        i, q = [rng.randrange(-32768, 32768) for _ in range(2)]
    y, x = i*cx + q*cy, i*cy - q*cx
    golden = max(-65536, min(65535, y // fraction))
    valid = x > 341782528 and 4*abs(y) < x
    rows.append((cx, cy, i, q, golden, int(valid), 0))
Path(sys.argv[1]).write_text("".join(" ".join(map(str, row)) + "\n" for row in rows))
print(f"Generated {len(rows)} P detector vectors")
