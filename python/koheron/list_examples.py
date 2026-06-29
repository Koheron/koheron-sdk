#!/usr/bin/env python3
"""List available Koheron example instruments."""

from __future__ import annotations

import argparse
import re
from pathlib import Path

_ASSIGNMENT_RE = re.compile(r"^\s*(NAME|VERSION|BOARD_PATH)\s*(?::=|\?=|=)\s*(.*?)\s*$")


def parse_config(config_path: Path) -> dict[str, str]:
    """Return selected Make variables from a config.mk file."""
    values: dict[str, str] = {}
    with config_path.open("r", encoding="utf-8") as config_file:
        for line in config_file:
            line_without_comment = line.partition("#")[0]
            match = _ASSIGNMENT_RE.match(line_without_comment)
            if match:
                key, value = match.groups()
                values[key] = value.strip()
    return values


def relative_posix(path: Path, root: Path) -> str:
    """Return path relative to root, formatted with POSIX separators."""
    return path.relative_to(root).as_posix()


def list_examples(sdk_path: Path) -> None:
    """Print all example instruments found under examples/*/*/config.mk."""
    sdk_path = sdk_path.resolve()
    config_paths = sorted((sdk_path / "examples").glob("*/*/config.mk"))

    print("Available example instruments:")
    print()

    if not config_paths:
        print("  none found")
        return

    for config_path in config_paths:
        instrument_path = config_path.parent.relative_to(sdk_path / "examples").as_posix()
        values = parse_config(config_path)

        print(f"  {instrument_path}")
        print(f"    CFG       {relative_posix(config_path, sdk_path)}")
        if "NAME" in values:
            print(f"    NAME      {values['NAME']}")
        if "VERSION" in values:
            print(f"    VERSION   {values['VERSION']}")
        if "BOARD_PATH" in values:
            print(f"    BOARD     {values['BOARD_PATH']}")
        print()


def main() -> None:
    parser = argparse.ArgumentParser(description="List available Koheron example instruments.")
    parser.add_argument(
        "--sdk-path",
        default=".",
        help="Path to the Koheron SDK repository root (default: current directory).",
    )
    args = parser.parse_args()

    list_examples(Path(args.sdk_path))


if __name__ == "__main__":
    main()
