"""Minimal TCP client for NetworkMappedMemory servers.

This script connects to a NetworkMappedMemory TCP server and prints simple
throughput metrics while receiving data.  Example usage::

    python -m koheron.network_mapped_memory_client --host 192.168.0.10 --port 9000

"""

from __future__ import annotations

import argparse
import socket
import time
from typing import Tuple


def format_rate(bytes_per_second: float) -> str:
    """Format a byte/s throughput value using binary units."""
    units: Tuple[str, ...] = ("B/s", "KiB/s", "MiB/s", "GiB/s")
    value = bytes_per_second

    for unit in units:
        if value < 1024.0 or unit == units[-1]:
            return f"{value:.2f} {unit}"
        value /= 1024.0

    return f"{value:.2f} {units[-1]}"


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="127.0.0.1", help="Server hostname or IP address")
    parser.add_argument("--port", type=int, required=True, help="Server TCP port")
    parser.add_argument(
        "--duration",
        type=float,
        default=None,
        help="Optional duration (seconds) to run before disconnecting",
    )
    parser.add_argument(
        "--chunk-size",
        type=int,
        default=256 * 1024,
        help="Receive buffer size in bytes (default: 256 KiB)",
    )
    parser.add_argument(
        "--report-interval",
        type=float,
        default=10.0,
        help="Seconds between throughput reports (default: 1.0)",
    )
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)

    with socket.create_connection((args.host, args.port)) as sock:
        sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        sock.settimeout(args.report_interval)

        total_bytes = 0
        last_bytes = 0
        start_time = time.perf_counter()
        last_report = start_time

        try:
            while True:
                try:
                    chunk = sock.recv(args.chunk_size)
                except socket.timeout:
                    chunk = b""

                now = time.perf_counter()
                elapsed = now - start_time

                if chunk:
                    total_bytes += len(chunk)
                else:
                    # No data within timeout; still emit metrics.
                    pass

                if not chunk:
                    if args.duration is None and chunk == b"":
                        # Server closed the connection.
                        break

                if args.duration is not None and elapsed >= args.duration:
                    break

                if now - last_report >= args.report_interval:
                    interval_bytes = total_bytes - last_bytes
                    interval = now - last_report
                    avg_rate = total_bytes / max(elapsed, 1e-9)
                    inst_rate = interval_bytes / max(interval, 1e-9)
                    print(
                        f"t={elapsed:6.2f}s | total={total_bytes} bytes | "
                        f"avg={format_rate(avg_rate)} | interval={format_rate(inst_rate)}"
                    )
                    last_report = now
                    last_bytes = total_bytes

        except KeyboardInterrupt:
            pass

        # Final report
        end_time = time.perf_counter()
        elapsed = max(end_time - start_time, 1e-9)
        avg_rate = total_bytes / elapsed
        print(
            f"Finished after {elapsed:.2f}s: received {total_bytes} bytes "
            f"(average {format_rate(avg_rate)})"
        )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())