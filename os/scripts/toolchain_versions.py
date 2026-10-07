#!/usr/bin/env python3
"""Report the selected Xilinx installations without inferring versions from paths."""
import argparse
import os
from pathlib import Path
import re
import subprocess
import sys


def tool_version(path, product, option):
    executable = Path(path) / 'bin' / product
    environment = dict(os.environ)
    # The SDK recipe error trap must not run inside vendor launchers.
    environment.pop('BASH_ENV', None)
    try:
        result = subprocess.run(
            [str(executable), option], capture_output=True, text=True,
            errors='replace', env=environment, timeout=10, check=True)
    except (OSError, subprocess.SubprocessError) as error:
        print(f'[WARN] Cannot query {product} version: {error}; recording unknown',
              file=sys.stderr)
        return 'unknown', 'unknown'

    output = result.stdout + '\n' + result.stderr
    version = re.search(
        rf'^\s*(?:\*+\s*)?{product}\s+v(\d{{4}}\.\d+(?:\.\d+)?)\b',
        output, re.MULTILINE | re.IGNORECASE)
    build = re.search(r'^\s*(?:\*+\s*)?SW Build\s+(\d+)\b',
                      output, re.MULTILINE | re.IGNORECASE)
    if not version:
        print(f'[WARN] Unrecognized {product} version output; recording unknown',
              file=sys.stderr)
        return 'unknown', 'unknown'
    if not build:
        print(f'[WARN] {product} software build number unavailable; recording unknown',
              file=sys.stderr)
    return version.group(1), build.group(1) if build else 'unknown'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--vivado-path', required=True)
    parser.add_argument('--vitis-path', required=True)
    args = parser.parse_args()
    for product, path, option in (
        ('vivado', args.vivado_path, '-version'),
        ('vitis', args.vitis_path, '--version'),
    ):
        version, build = tool_version(path, product, option)
        print(f'{product}={version}')
        print(f'{product}_build={build}')


if __name__ == '__main__':
    main()
