"""Write compatibility metadata for an instrument ZIP (host build tool)."""
import argparse
import json
from pathlib import Path
import os
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--board', required=True)
    parser.add_argument('--architecture', choices=('armhf', 'arm64'), required=True)
    parser.add_argument('--sdk-version', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    content = (json.dumps({'format': 1, 'board': args.board,
        'architecture': args.architecture, 'sdk_version': args.sdk_version,
        'min_runtime_api': 1}, sort_keys=True, indent=2) + '\n').encode()
    if args.output.exists() and args.output.read_bytes() == content:
        return
    args.output.parent.mkdir(parents=True, exist_ok=True)
    descriptor, name = tempfile.mkstemp(prefix='.instrument-', dir=args.output.parent)
    try:
        with os.fdopen(descriptor, 'wb') as output:
            output.write(content)
            os.fchmod(output.fileno(), 0o644)
        os.replace(name, args.output)
    finally:
        if os.path.exists(name):
            os.unlink(name)


if __name__ == '__main__':
    main()
