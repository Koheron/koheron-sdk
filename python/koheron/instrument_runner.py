"""Utilities to deploy an instrument and stream Koheron logs."""

import argparse
import sys
from typing import Optional

import requests

from .koheron import (
    upload_instrument,
    run_instrument,
    instrument_logs_bookmark,
    stream_instrument_logs,
)

GREEN = "\033[1;32m"
RESET = "\033[0m"


def _parse_args(argv) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("instrument_zip", help="Path to the instrument zip archive")
    parser.add_argument(
        "--host",
        required=True,
        help="Host IP address or hostname of the Koheron board",
    )
    parser.add_argument(
        "--name",
        required=True,
        help="Instrument name to start after upload",
    )
    parser.add_argument(
        "--poll",
        type=float,
        default=1.0,
        help="HTTP fallback polling interval in seconds (logs use WebSocket by default)",
    )
    return parser.parse_args(argv)


def _safe_instrument_logs_bookmark(host: str) -> Optional[str]:
    try:
        return instrument_logs_bookmark(host)
    except requests.RequestException as exc:
        print(f"[log-stream] Unable to bookmark instrument logs: {exc}", file=sys.stderr)
        return None


def main(argv=None) -> int:
    args = _parse_args(argv)

    print(f"{GREEN}Uploading {args.instrument_zip} to {args.host}...{RESET}")
    upload_instrument(args.host, args.instrument_zip, run=False)

    print(f"{GREEN}Starting instrument '{args.name}'...{RESET}")
    run_instrument(args.host, args.name, restart=True)

    bookmark = _safe_instrument_logs_bookmark(args.host)

    print(f"{GREEN}Streaming logs (press Ctrl+C to stop)...{RESET}")
    try:
        stream_instrument_logs(args.host, cursor=bookmark, poll_interval=args.poll)
    except KeyboardInterrupt:
        print("\nLog streaming interrupted.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
