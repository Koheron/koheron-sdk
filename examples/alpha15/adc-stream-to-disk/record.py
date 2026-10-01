#!/usr/bin/env python3
"""Record ALPHA15 ADC0 samples on the PC over the SDK's plain TCP connection."""

import argparse
import json
import math
import os
import signal
import time

from koheron import command, connect


WORDS_PER_CHUNK = 128 * 1024
BYTES_PER_CHUNK = 4 * WORDS_PER_CHUNK
WORDS_PER_BATCH = 4 * WORDS_PER_CHUNK
BYTES_PER_BATCH = 4 * BYTES_PER_CHUNK
ADC_BYTES_PER_SECOND = 15_000_000 * 4
ERRORS = {
    1: "DMA reset timed out",
    2: "capture is not running",
    3: "DMA reported an error",
    4: "DMA descriptor timed out",
    5: "DMA ring filled",
    6: "DMA descriptor was short",
    7: "ADC samples were lost to backpressure",
}


def check_error(code):
    if code:
        raise RuntimeError(f"ADC capture failed: {ERRORS.get(code, 'unknown error')} (code {code})")


class AdcStream:
    def __init__(self, client):
        self.client = client

    @command()
    def start(self):
        return self.client.recv_uint32()

    @command()
    def stop(self):
        pass

    @command()
    def read_batch(self):
        return self.client.recv_array(WORDS_PER_BATCH, dtype="uint32")

    @command()
    def get_error(self):
        return self.client.recv_uint32()

    @command()
    def set_test_tone(self, enabled):
        pass


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("host", help="ALPHA15 IP address or hostname")
    parser.add_argument("output", help="output file (raw little-endian uint32)")
    parser.add_argument("--seconds", type=float,
                        help="capture duration (default: 10 seconds, or until Ctrl+C in ring mode)")
    parser.add_argument("--ring-seconds", type=float,
                        help="keep only the most recent N seconds in a fixed-size disk file")
    parser.add_argument("--test-tone", action="store_true",
                        help="drive DAC0 with a 7.324 kHz triangle for loopback tests")
    args = parser.parse_args()
    if args.seconds is not None and args.seconds <= 0:
        parser.error("--seconds must be positive")
    if args.ring_seconds is not None and args.ring_seconds <= 0:
        parser.error("--ring-seconds must be positive")

    duration = args.seconds if args.seconds is not None else (None if args.ring_seconds else 10.0)
    ring_batches = (math.ceil(args.ring_seconds * ADC_BYTES_PER_SECOND / BYTES_PER_BATCH)
                    if args.ring_seconds else None)

    client = connect(args.host, name="adc-stream-to-disk")
    adc = AdcStream(client)
    count = 0
    partial = args.output + ".partial"
    stop_requested = False

    def request_stop(_signal, _frame):
        nonlocal stop_requested
        stop_requested = True

    previous_handler = signal.signal(signal.SIGINT, request_stop)

    try:
        with open(partial, "w+b" if ring_batches else "wb") as output:
            if ring_batches:
                os.posix_fallocate(output.fileno(), 0, ring_batches * BYTES_PER_BATCH)
            try:
                adc.set_test_tone(1 if args.test_tone else 0)
                check_error(adc.start())
                started = time.monotonic()
                if ring_batches and duration is None:
                    print(f"Keeping the latest {ring_batches * BYTES_PER_BATCH / ADC_BYTES_PER_SECOND:.2f} "
                          "seconds on disk; press Ctrl+C to stop", flush=True)
                while not stop_requested and (duration is None or time.monotonic() - started < duration):
                    batch = adc.read_batch()
                    if ring_batches and count and count % ring_batches == 0:
                        output.seek(0)
                    batch.tofile(output)
                    count += 1
                    if count % 2 == 0:
                        check_error(adc.get_error())
                check_error(adc.get_error())
            finally:
                adc.stop()
                adc.set_test_tone(0)

        elapsed = time.monotonic() - started
        if ring_batches:
            metadata = {
                "format": "alpha15-adc0-u32-sequence14",
                "batch_bytes": BYTES_PER_BATCH,
                "capacity_batches": ring_batches,
                "completed_batches": count,
                "sample_rate_hz": 15_000_000,
            }
            metadata_partial = partial + ".json"
            with open(metadata_partial, "w", encoding="utf-8") as meta:
                json.dump(metadata, meta, indent=2)
                meta.write("\n")
            os.replace(partial, args.output)
            os.replace(metadata_partial, args.output + ".json")
            retained = min(count, ring_batches) * BYTES_PER_BATCH
            received = count * BYTES_PER_BATCH
            print(f"Retained {retained:,} bytes ({retained / ADC_BYTES_PER_SECOND:.2f} seconds) "
                  f"from {received:,} received bytes in {elapsed:.2f} s "
                  f"({received / elapsed / 1e6:.1f} MB/s)")
        else:
            os.replace(partial, args.output)
            metadata_path = args.output + ".json"
            if os.path.exists(metadata_path):
                os.unlink(metadata_path)
            print(f"Saved {count * BYTES_PER_BATCH} bytes in {elapsed:.2f} s "
                  f"({count * BYTES_PER_BATCH / elapsed / 1e6:.1f} MB/s)")
    finally:
        signal.signal(signal.SIGINT, previous_handler)


if __name__ == "__main__":
    main()
