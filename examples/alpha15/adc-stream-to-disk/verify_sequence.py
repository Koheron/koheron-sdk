#!/usr/bin/env python3
"""Check every ADC conversion tag in a recorder output file."""

import argparse
import json
import os

import numpy as np


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", help="raw file produced by record.py")
    args = parser.parse_args()

    size = os.path.getsize(args.capture)
    if size == 0 or size % 4:
        parser.error("capture must contain a nonzero whole number of uint32 words")

    words = np.memmap(args.capture, dtype="<u4", mode="r")
    metadata_path = args.capture + ".json"
    if os.path.exists(metadata_path):
        with open(metadata_path, encoding="utf-8") as stream:
            metadata = json.load(stream)
        if metadata.get("format") != "alpha15-adc0-u32-sequence14":
            parser.error("unrecognized ring format")
        batch_bytes = metadata["batch_bytes"]
        capacity = metadata["capacity_batches"]
        completed = metadata["completed_batches"]
        if (batch_bytes <= 0 or batch_bytes % 4 or capacity <= 0 or completed <= 0
                or size != batch_bytes * capacity):
            parser.error("invalid ring metadata or file size")
        valid_words = min(completed, capacity) * batch_bytes // 4
        start = ((completed % capacity) * batch_bytes // 4
                 if completed >= capacity else 0)
        spans = ((start, len(words)), (0, start)) if completed >= capacity else ((0, valid_words),)
    else:
        valid_words = len(words)
        spans = ((0, len(words)),)

    chunk_words = 8 * 1024 * 1024
    previous = None
    checked = 0
    for begin, end in spans:
        for offset in range(begin, end, chunk_words):
            sequence = np.asarray(words[offset:min(offset + chunk_words, end)] >> 18,
                                  dtype=np.uint16)
            if previous is not None and sequence[0] != ((previous + 1) & 0x3fff):
                raise SystemExit(
                    f"Sequence gap at chronological sample {checked}: "
                    f"expected {(previous + 1) & 0x3fff}, got {int(sequence[0])}"
                )
            bad = np.flatnonzero(((sequence[:-1].astype(np.uint32) + 1) & 0x3fff)
                                != sequence[1:])
            if bad.size:
                index = checked + int(bad[0]) + 1
                raise SystemExit(
                    f"Sequence gap at chronological sample {index}: expected "
                    f"{(int(sequence[bad[0]]) + 1) & 0x3fff}, "
                    f"got {int(sequence[bad[0] + 1])}"
                )
            previous = int(sequence[-1])
            checked += len(sequence)

    print(f"PASS: {checked:,} consecutive ADC samples ({checked * 4:,} bytes), "
          "no sequence gaps")


if __name__ == "__main__":
    main()
