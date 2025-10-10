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
        help="Polling interval (in seconds) between log fetches",
    )
    return parser.parse_args(argv)


def _safe_instrument_logs_bookmark(host: str) -> Optional[str]:
    try:
        return instrument_logs_bookmark(host)
    except requests.RequestException as exc:
        print(
            "[log-stream] Unable to bookmark instrument logs: {}".format(exc),
            file=sys.stderr,
        )
        return None


def main(argv=None) -> int:
    if argv is None:
        argv = sys.argv[1:]
    args = _parse_args(argv)

    print("Uploading {} to {}...".format(args.instrument_zip, args.host))
    upload_instrument(args.host, args.instrument_zip, run=False)

    print("Starting instrument '{}'...".format(args.name))
    run_instrument(args.host, args.name, restart=True)

    bookmark = _safe_instrument_logs_bookmark(args.host)

    print("Streaming logs (press Ctrl+C to stop)...")
    try:
        stream_instrument_logs(args.host, cursor=bookmark, poll_interval=args.poll)
    except KeyboardInterrupt:
        print("\nLog streaming interrupted.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
