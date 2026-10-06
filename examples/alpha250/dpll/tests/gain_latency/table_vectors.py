#!/usr/bin/env python3
"""Exercise background table loading and atomic switches against integer math."""
import argparse
import math
import random
from pathlib import Path


def signed(value, width):
    value &= (1 << width) - 1
    return value - (1 << width) if value & (1 << (width-1)) else value


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    parser.add_argument("--chunk-bits", type=int, default=6)
    parser.add_argument("--fraction-bits", type=int, default=11)
    args = parser.parse_args()
    rng = random.Random(671091)
    mantissas = [round(2 ** (args.fraction_bits + j/16)) for j in range(16)]
    error = max(abs(c / 2 ** (args.fraction_bits+j/16)-1) for j,c in enumerate(mantissas))
    assert error <= 0.005
    gains = [polarity*c*(1 << octave)
             for octave in range(32) for c in mantissas for polarity in [-1,1]]
    rng.shuffle(gains)
    gains = [0, *gains, 0]
    bank = 0
    active_gain = 0
    count = 0
    edges = [0, 1, -1]
    for width in [17, 32, 48]:
        edges += [-(1 << (width-1)), (1 << (width-1))-1, (1 << width)-1]
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w") as f:
        def emit(we=0, target=0, top=0, address=0, data=0, a=None):
            nonlocal count
            if a is None:
                a = rng.getrandbits(48)
            expected = 0
            bit = 0
            for width, low, out in [(17,0,32), (32,16,32), (48,48,32), (32,0,64)]:
                y = signed(a,width)*active_gain // (1 << (args.fraction_bits+low))
                expected |= (y & ((1 << out)-1)) << bit
                bit += out
            packed = a & ((1 << 48)-1)
            for value,width in [(we,1), (target,1), (top,1), (address,8),
                                (data,64), (bank,1), (expected,160)]:
                packed = (packed << width) | (value & ((1 << width)-1))
            f.write(f"{packed:071x}\n")
            count += 1
        for gain in gains:
            target = 1-bank
            for top in [0,1]:
                for address in range(1 << args.chunk_bits):
                    factor = signed(address,args.chunk_bits) if top else address
                    emit(1,target,top,address,gain*factor)
            # The previous write is sampled before this commit vector.
            bank = target
            active_gain = gain
            for a in edges:
                emit(a=a)
            for _ in range(4):
                emit()
    print(f"Table gain vectors: {count}; 16 steps/octave; coefficients={mantissas}")
    print(f"Maximum coefficient error: {100*error:.9f}%; fraction bits={args.fraction_bits}")


if __name__ == "__main__":
    main()
