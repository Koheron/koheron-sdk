#!/usr/bin/env python3
"""Copy an instrument's sources with a new package name, without build tools."""

from __future__ import annotations

import argparse
import re
import shutil
import sys
from pathlib import Path

_NAME_RE = re.compile(r"[A-Za-z0-9][A-Za-z0-9_-]*")
_NAME_LINE_RE = re.compile(r"^[ \t]*(?:(?:override|export)[ \t]+)?NAME[ \t]*[:?+!]?=", re.MULTILINE)
_NAME_ASSIGNMENT_RE = re.compile(
    r"^([ \t]*NAME[ \t]*(?::=|\?=|=)[ \t]*)([A-Za-z0-9][A-Za-z0-9_-]*)"
    r"([ \t]*(?:#[^\r\n]*)?)(\r?\n|$)",
    re.MULTILINE,
)
_IGNORE = shutil.ignore_patterns(
    ".git", ".venv", "__pycache__", "*.pyc", "*.pyo",
    "node_modules", ".Xil", ".cache", ".pytest_cache",
)


def copy_instrument(sdk_path: Path, cfg: Path, destination: Path) -> Path:
    """Copy the CFG directory, refusing overwrites and recursive destinations."""
    sdk_path = sdk_path.resolve()
    cfg = sdk_path / cfg
    destination = sdk_path / destination
    if cfg.name != "config.mk" or not cfg.is_file():
        raise ValueError(f"CFG must reference an existing config.mk: {cfg}")
    if destination.exists() or destination.is_symlink():
        raise ValueError(f"Destination already exists: {destination}")
    name = destination.name
    if not _NAME_RE.fullmatch(name):
        raise ValueError(
            "Destination name must start with a letter or digit and contain "
            "only letters, digits, '-' or '_'."
        )

    source = cfg.parent.resolve()
    destination = destination.resolve()
    if destination == source or source in destination.parents:
        raise ValueError("Destination must be outside the source instrument directory.")

    config = cfg.read_bytes().decode("utf-8")
    assignments = list(_NAME_ASSIGNMENT_RE.finditer(config))
    if len(assignments) != 1 or len(_NAME_LINE_RE.findall(config)) != 1:
        raise ValueError("Source config.mk must contain one literal NAME assignment.")
    assignment = assignments[0]
    if name == assignment.group(2):
        raise ValueError("Choose a destination name different from the source instrument's NAME.")
    config = config[:assignment.start(2)] + name + config[assignment.end(2):]

    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.mkdir()  # Reserve the destination exclusively; never overwrite it.
    try:
        shutil.copytree(source, destination, symlinks=True, ignore=_IGNORE, dirs_exist_ok=True)
        copied_cfg = destination / "config.mk"
        if copied_cfg.is_symlink():
            copied_cfg.unlink()  # Do not modify the original through a copied symlink.
        copied_cfg.write_bytes(config.encode("utf-8"))
    except (OSError, UnicodeError):
        shutil.rmtree(destination)
        raise
    return copied_cfg


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk-path", default=".", help="Path to the Koheron SDK repository")
    parser.add_argument("--cfg", required=True, help="Source instrument config.mk, relative to SDK_PATH")
    parser.add_argument("--destination", required=True, help="New instrument directory, relative to SDK_PATH")
    args = parser.parse_args()
    try:
        config = copy_instrument(Path(args.sdk_path), Path(args.cfg), Path(args.destination))
    except (OSError, ValueError) as error:
        print(f"Cannot copy instrument: {error}", file=sys.stderr)
        return 1
    print(f"Copied instrument to {config.parent}")
    print(f"NAME := {config.parent.name}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
