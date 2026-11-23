#!/usr/bin/env python3
"""List available Koheron example instruments."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

if __package__:
    from .sdk_info import load_settings
else:
    from sdk_info import load_settings


def relative_posix(path: Path, root: Path) -> str:
    """Return path relative to root, formatted with POSIX separators."""
    return path.relative_to(root).as_posix()


def find_configs(sdk_path: Path) -> list[Path]:
    """Discover config.mk and stemlab-*.mk entry points without evaluating them."""
    return sorted(path for path in (sdk_path / "examples").rglob("*.mk")
                  if path.name == "config.mk" or path.name.startswith("stemlab-"))


def list_examples(sdk_path: Path) -> int:
    """Print selectable instrument configurations, including board selectors."""
    sdk_path = sdk_path.resolve()
    config_paths = find_configs(sdk_path)

    print("Available example instruments:")
    print()

    if not config_paths:
        print("  none found")
        return 0

    errors = 0
    for config_path in config_paths:
        instrument_path = config_path.parent.relative_to(sdk_path / "examples").as_posix()
        if config_path.name != "config.mk":
            instrument_path += f" [{config_path.stem}]"
        print(f"  {instrument_path}")
        print(f"    CFG       {relative_posix(config_path, sdk_path)}")
        try:
            values = load_settings(sdk_path, config_path)
        except ValueError as error:
            print(f"Cannot resolve {relative_posix(config_path, sdk_path)}: {error}", file=sys.stderr)
            errors += 1
        else:
            for name in ('NAME', 'VERSION', 'BOARD'):
                if values.get(name):
                    print(f"    {name:<10}{values[name]}")
        print()
    return 1 if errors else 0


def main() -> int:
    parser = argparse.ArgumentParser(description="List available Koheron example instruments.")
    parser.add_argument(
        "--sdk-path",
        default=".",
        help="Path to the Koheron SDK repository root (default: current directory).",
    )
    parser.add_argument("--configs", action="store_true",
                        help="Print only selectable configuration paths, for checks and CI")
    args = parser.parse_args()
    sdk_path = Path(args.sdk_path)
    if args.configs:
        for path in find_configs(sdk_path):
            print(path.as_posix())
        return 0
    return list_examples(sdk_path)


if __name__ == "__main__":
    raise SystemExit(main())
