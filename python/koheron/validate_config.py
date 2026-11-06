#!/usr/bin/env python3
"""Fast instrument configuration validation for Koheron SDK builds."""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

if __package__:
    from .diagnostics import Reporter
    from .memory_schema import parse_int, parse_range
    from .sdk_info import load_settings, read_settings
else:
    # Direct script execution and importlib-based SDK fixtures avoid loading
    # the client package and its unrelated runtime dependencies.
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from diagnostics import Reporter
    from memory_schema import parse_int, parse_range
    from sdk_info import load_settings, read_settings

try:
    import yaml  # type: ignore
except ImportError:  # pragma: no cover - PyYAML is listed in SDK requirements.
    yaml = None

ARRAY_RE = re.compile(r"\[\s*([^\]]+)\s*\]")


def validate_config(reporter: Reporter, values: dict[str, str], sdk_path: Path) -> None:
    """Check the final inputs selected by Make, including command-line overrides."""
    def check_path(label: str, value: str) -> None:
        path = Path(value)
        if not path.is_absolute():
            path = sdk_path / path
        if value and path.exists():
            reporter.ok(f"{label} exists: {path}")
        else:
            reporter.err(f"{label} missing: {value or '(unset)'}")

    check_path('CFG', values['CONFIG_MK'])
    for name in ('NAME', 'VERSION', 'BOARD_PATH'):
        if values.get(name, '').strip():
            reporter.ok(f"{name} is defined")
        else:
            reporter.err(f"{name} is missing or empty")
    check_path('BOARD_MK', values.get('BOARD_MK', ''))
    for name in ('DRIVERS', 'XDC'):
        for value in values.get(name, '').split():
            check_path(f'{name} file', value)


def validate_memory(reporter: Reporter, memory_yml: Path) -> None:
    if not memory_yml.exists():
        reporter.err(f"Selected memory.yml missing: {memory_yml}")
        return
    reporter.ok(f"memory.yml exists: {memory_yml}")
    if yaml is None:
        reporter.err("PyYAML is required to validate memory.yml; run make python_requirements")
        return
    try:
        data = yaml.safe_load(memory_yml.read_text(encoding="utf-8"))
    except Exception as exc:  # noqa: BLE001 - report parser details without traceback.
        reporter.err(f"memory.yml could not be parsed: {exc}")
        return
    if not isinstance(data, dict):
        reporter.err("memory.yml top-level YAML must be a mapping")
        return
    reporter.ok("memory.yml top-level YAML is a mapping")
    memory = data.get("memory")
    if not isinstance(memory, list):
        reporter.err("memory.yml 'memory' must be present and be a list")
        return
    reporter.ok("memory.yml memory is a list")

    parameters = data.get("parameters", {})
    if parameters is None:
        parameters = {}
    if not isinstance(parameters, dict):
        reporter.err("memory.yml parameters must be a mapping when present")
        parameters = {}

    names: set[str] = set()
    ranges: list[tuple[int, int, str]] = []
    for index, entry in enumerate(memory):
        label = f"memory[{index}]"
        if not isinstance(entry, dict):
            reporter.err(f"{label} must be a mapping")
            continue
        name = entry.get("name")
        if isinstance(name, str) and name:
            label = name
            if name in names:
                reporter.err(f"duplicate memory name: {name}")
            else:
                names.add(name)
                reporter.ok(f"memory entry has unique name: {name}")
        else:
            reporter.err(f"{label} has missing or empty name")
        offset = parse_int(entry.get("offset"))
        size = parse_range(entry.get("range"))
        if offset is None:
            reporter.err(f"{label} offset is missing or invalid")
        else:
            reporter.ok(f"{label} offset parses as integer")
        if size is None:
            reporter.err(f"{label} range is missing or invalid")
        else:
            reporter.ok(f"{label} range parses as integer")
        if offset is not None and size is not None:
            ranges.append((offset, offset + size, label))
        if "dev" in entry:
            if isinstance(entry["dev"], str):
                reporter.ok(f"{label} dev is a string")
            else:
                reporter.err(f"{label} dev must be a string when present")
        registers = entry.get("registers")
        if registers is None:
            continue
        if not isinstance(registers, list):
            reporter.err(f"{label} registers must be a list when present")
            continue
        reporter.ok(f"{label} registers is a list")
        for register in registers:
            if not isinstance(register, str):
                reporter.err(f"{label} register entries must be strings")
                continue
            for expr in ARRAY_RE.findall(register):
                count = parse_int(expr)
                if count is not None:
                    if count > 0:
                        reporter.ok(f"{register} register count is positive")
                    else:
                        reporter.err(f"{register} register count must be > 0")
                else:
                    if expr not in parameters:
                        reporter.err(f"{register} references undefined parameter '{expr}'")
                    else:
                        param_count = parse_int(parameters[expr])
                        if param_count is not None and param_count > 0:
                            reporter.ok(f"{register} uses positive parameter '{expr}'")
                        else:
                            reporter.err(f"{register} parameter '{expr}' must be a positive integer")
    for i, (start, end, name) in enumerate(ranges):
        for other_start, other_end, other_name in ranges[i + 1 :]:
            if start < other_end and other_start < end:
                reporter.warn(f"memory ranges overlap: {name} and {other_name}")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Validate one Koheron instrument CFG and memory.yml.")
    parser.add_argument("--sdk-path", default=".", help="Path to the Koheron SDK repository")
    selection = parser.add_mutually_exclusive_group(required=True)
    selection.add_argument("--cfg", help="Path to an instrument config.mk")
    selection.add_argument('--settings-stdin', action='store_true',
                           help='Validate settings already resolved by Make')
    parser.add_argument('--memory-yml', type=Path, help='Resolved MEMORY_YML override')
    parser.add_argument("--no-color", action="store_true", help="Disable colored status labels")
    args = parser.parse_args(argv)

    reporter = Reporter(color=sys.stdout.isatty() and not args.no_color)
    sdk_path = Path(args.sdk_path).resolve()
    try:
        values = read_settings(sys.stdin) if args.settings_stdin else load_settings(sdk_path, args.cfg)
    except ValueError as error:
        reporter.err(f'Cannot resolve configuration: {error}')
        return 1
    validate_config(reporter, values, sdk_path)
    memory_yml = args.memory_yml if args.memory_yml is not None else Path(values['MEMORY_YML'])
    if not memory_yml.is_absolute():
        memory_yml = sdk_path / memory_yml
    validate_memory(reporter, memory_yml)
    return 1 if reporter.errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
