#!/usr/bin/env python3
"""Independent atan2 reference, including every small IQ pair and phase cuts."""
import math
import random
import sys

rng = random.Random(719337)
points = [(x, y) for x in range(-32, 33) for y in range(-32, 33)]
edges = [-8388608, -8388607, -4194304, -32768, -2, -1, 0, 1, 2,
         32767, 4194304, 8388606, 8388607]
points += [(x, y) for x in edges for y in edges]
# Exercise every signed carry boundary used by direct leading-bit detection.
for bit in range(24):
    for sign in [-1, 1]:
        for delta in [-1, 0, 1]:
            value = sign * (1 << bit) + delta
            if -8388608 <= value <= 8388607:
                points += [(value, 0), (0, value), (value, 1), (1, value),
                           (value, -1), (-1, value), (value, value)]
for amplitude in [1, 2, 7, 31, 127, 511, 2047, 8191, 16383, 32767,
                  65535, 131071, 524287, 2097151, 4194303, 8388607]:
    for k in range(8192):
        theta = 2 * math.pi * k / 8192
        points.append((round(amplitude * math.cos(theta)), round(amplitude * math.sin(theta))))
for _ in range(65536):
    points.append((rng.randrange(-8388608, 8388608), rng.randrange(-8388608, 8388608)))
# Distinct angles carried entirely in the lower eight Cartesian bits must
# survive the extractor interface, including signed inputs and phase cuts.
points += [(x, y) for x in [1024, -1024, 768000, -768000]
           for y in [0, 1, -1, 64, -64, 128, -128, 255, -255, 640, 760]]

with open(sys.argv[1], "w", encoding="ascii") as stream:
    for index, (x, y) in enumerate(points):
        phase = math.atan2(y, x) * 8192 / math.pi if x or y else 0.0
        expected = math.floor(phase + 0.5)
        # Bubbles and two-cycle resets check metadata flushing.
        resetn = int(index % 4096 not in (101, 102))
        valid = int(index % 23 != 8)
        stream.write(f"{x} {y} {expected} {valid} {resetn} {phase:.12f}\n")
print(f"Generated {len(points)} independent atan2 vectors")
