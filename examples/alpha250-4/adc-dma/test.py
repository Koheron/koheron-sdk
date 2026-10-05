#!/usr/bin/env python3
"""Acquire 16 million samples per input; optionally verify every counter code."""
import argparse
import os
from pathlib import Path
import time

import numpy as np
from koheron import connect
from adc_dma import AdcDma, DEFAULT_SAMPLES, SAMPLE_RATE, validate_samples


def capture(driver, args, path):
    # Reserve the filename before writing; a concurrent run cannot overwrite it.
    if path is not None:
        with path.open("xb"):
            pass
    try:
        output = (np.lib.format.open_memmap(path, mode="w+", dtype="<i2", shape=(args.samples, 4))
                  if path is not None else None)
        started = time.monotonic()
        data = driver.acquire(args.samples, args.test_pattern, args.timeout, output=output)
        elapsed = time.monotonic() - started
        if output is not None:
            output.flush()
        ranges = [(int(channel.min()), int(channel.max())) for channel in data.T]
        return elapsed, ranges
    except BaseException:
        if path is not None:
            # An incomplete record must not look like a successful capture.
            path.unlink(missing_ok=True)
        raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default=os.getenv("HOST"), required=not bool(os.getenv("HOST")))
    parser.add_argument("--samples", type=int, default=DEFAULT_SAMPLES)
    parser.add_argument("--test-pattern", action="store_true", help="FPGA counters instead of ADC codes; verify every sample")
    parser.add_argument("--repeat", type=int, default=1)
    parser.add_argument("--timeout", type=float, default=5.0)
    parser.add_argument("--output", type=Path, help="Save (samples, 4) int16 array as .npy; existing files are refused")
    args = parser.parse_args()
    try:
        validate_samples(args.samples)
    except ValueError as error:
        parser.error(str(error))
    if args.repeat < 1:
        parser.error("--repeat must be positive")
    if args.timeout <= 0:
        parser.error("--timeout must be positive")
    if args.output and args.output.suffix != ".npy":
        parser.error("--output must end in .npy")
    outputs = [None] * args.repeat
    if args.output is not None:
        outputs = [args.output] if args.repeat == 1 else [
            args.output.with_name(f"{args.output.stem}-{i + 1:03d}.npy")
            for i in range(args.repeat)
        ]
    for path in outputs:
        if path is not None and path.exists():
            parser.error(f"Output already exists: {path}")
    client = connect(args.host, "adc-dma", restart=False)
    try:
        # Bound individual RPCs as well as the acquisition polling loop.
        client.sock.settimeout(args.timeout)
        driver = AdcDma(client)
        print(f"Four channels, {SAMPLE_RATE / 1e6:g} MS/s, {args.samples:,} samples/channel, "
              f"{args.samples / SAMPLE_RATE * 1000:.3f} ms capture")
        for i, path in enumerate(outputs):
            elapsed, ranges = capture(driver, args, path)
            print(f"Capture {i + 1}/{args.repeat}: all DMA descriptors checked, "
                  f"{'every pattern code verified, ' if args.test_pattern else ''}"
                  f"acquisition + download {elapsed:.2f} s")
            for channel, (minimum, maximum) in enumerate(ranges):
                print(f"  IN{channel}: min={minimum}, max={maximum}")
    finally:
        client.sock.close()


if __name__ == "__main__":
    main()
