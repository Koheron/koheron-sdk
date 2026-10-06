#!/usr/bin/env python3
"""Independent arbitrary-precision oracle for all four DPLL output slices."""
import argparse
import math
import random
from pathlib import Path


def signed(value, width):
    value &= (1 << width) - 1
    return value - (1 << width) if value & (1 << (width - 1)) else value


def expected(a, coefficient, octave):
    result = 0
    offset = 0
    for width, low, out_width in [(17, 0, 32), (32, 16, 32), (48, 48, 32), (32, 0, 64)]:
        # Python // floors negative values, matching arithmetic right shift.
        y = signed(a, width) * coefficient * (1 << octave) // (1 << (11 + low))
        result |= (y & ((1 << out_width) - 1)) << offset
        offset += out_width
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    parser.add_argument("--sparse", action="store_true", help="Use the <=0.5%% error sparse Q1.7 table")
    args = parser.parse_args()
    table = [round(2048 * 2 ** (j / 12)) for j in range(12)]
    if args.sparse:
        table = [c * 16 for c in [128, 136, 144, 152, 162, 171, 181, 192, 204, 216, 228, 242]]
    worst_error = max(abs(c / (2048 * 2 ** (j / 12)) - 1) for j, c in enumerate(table))
    worst_cents = max(abs(1200 * math.log2(c / (2048 * 2 ** (j / 12)))) for j, c in enumerate(table))
    values = {0, 1, -1}
    for width in [17, 24, 32, 48]:
        for v in [-(1 << (width - 1)), (1 << (width - 1)) - 1, (1 << (width - 1))]:
            for delta in [-1, 0, 1]:
                values.add(v + delta)
    vectors = []
    # Exhaustive octave/semitone/sign combinations at signed/chunk boundaries.
    for octave in range(32):
        for magnitude in [0, *table]:
            for polarity in [-1, 1]:
                for a in sorted(values):
                    vectors.append((a, polarity * magnitude, octave))
    rng = random.Random(97125)
    vectors.extend((rng.getrandbits(48), rng.choice([-1, 1]) * rng.choice(table + [0]), rng.randrange(32))
                   for _ in range(10000))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w") as f:
        for a, coefficient, octave in vectors:
            y = expected(a, coefficient, octave)
            packed = ((a & ((1 << 48) - 1)) << 178) | ((coefficient & 8191) << 165) | (octave << 160) | y
            f.write(f"{packed:057x}\n")
    print(f"Vectors: {len(vectors)}; table: {table}")
    print(f"Coefficient error: {100 * worst_error:.9f}% maximum, {worst_cents:.9f} cents maximum")


if __name__ == "__main__":
    main()
