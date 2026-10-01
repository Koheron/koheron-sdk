#!/usr/bin/env python3
"""Fast instrument configuration validation for Koheron SDK builds."""

from __future__ import annotations

import argparse
import os
import re
import sys
from pathlib import Path
from typing import Any

try:
    import yaml  # type: ignore
except ImportError:  # pragma: no cover - PyYAML is listed in SDK requirements.
    yaml = None

ASSIGN_RE = re.compile(r"^([A-Za-z_][A-Za-z0-9_]*)\s*([:+?]?=)\s*(.*)$")
INCLUDE_RE = re.compile(r"^-?include\s+(.+)$")
ARRAY_RE = re.compile(r"\[\s*([^\]]+)\s*\]")


class Reporter:
    def __init__(self, color: bool) -> None:
        self.color = color
        self.errors = 0
        self.warnings = 0

    def _line(self, level: str, message: str) -> None:
        colors = {"OK": "\033[1;32m", "WARN": "\033[1;33m", "ERR": "\033[1;31m"}
        reset = "\033[0m"
        label = f"[{level}]"
        if self.color:
            label = f"{colors[level]}{label}{reset}"
        print(f"{label} {message}")
        if level == "ERR":
            self.errors += 1
        elif level == "WARN":
            self.warnings += 1

    def ok(self, message: str) -> None:
        self._line("OK", message)

    def warn(self, message: str) -> None:
        self._line("WARN", message)

    def err(self, message: str) -> None:
        self._line("ERR", message)


def strip_comment(line: str) -> str:
    escaped = False
    out = []
    for char in line:
        if char == "#" and not escaped:
            break
        out.append(char)
        escaped = char == "\\" and not escaped
        if char != "\\":
            escaped = False
    return "".join(out).strip()


def join_make_lines(text: str) -> list[str]:
    lines: list[str] = []
    current = ""
    for raw in text.splitlines():
        line = raw.rstrip()
        if line.endswith("\\"):
            current += line[:-1] + " "
        else:
            lines.append(current + line)
            current = ""
    if current:
        lines.append(current)
    return lines


class MakeConfig:
    def __init__(self, sdk_path: Path, config_mk: Path) -> None:
        self.sdk_path = sdk_path
        self.config_mk = config_mk
        self.project_path = config_mk.parent
        self.variables: dict[str, str] = {
            "SDK_PATH": str(sdk_path),
            "PROJECT_PATH": str(self.project_path),
        }
        self.assignments: dict[str, list[tuple[Path, str]]] = {}
        self.includes: list[tuple[Path, str]] = []

    def parse(self) -> None:
        self._parse_file(self.config_mk, set())

    def _parse_file(self, path: Path, seen: set[Path]) -> None:
        path = path.resolve()
        if path in seen or not path.exists():
            return
        seen.add(path)
        try:
            lines = join_make_lines(path.read_text(encoding="utf-8"))
        except OSError:
            return
        for raw in lines:
            line = strip_comment(raw)
            if not line:
                continue
            include_match = INCLUDE_RE.match(line)
            if include_match:
                include_expr = include_match.group(1).strip()
                self.includes.append((path, include_expr))
                include_path = self.resolve_path(include_expr, path.parent)
                if include_path and include_path.exists():
                    self._parse_file(include_path, seen)
                continue
            assignment = ASSIGN_RE.match(line)
            if assignment:
                key, op, value = assignment.groups()
                value = value.strip()
                self.assignments.setdefault(key, []).append((path, value))
                old = self.variables.get(key, "")
                expanded = self.expand(value)
                if op == "+=":
                    self.variables[key] = f"{old} {expanded}".strip()
                elif op == "?=":
                    self.variables.setdefault(key, expanded)
                else:
                    self.variables[key] = expanded

    def expand(self, value: str) -> str:
        pattern = re.compile(r"\$\(([^)]+)\)|\$\{([^}]+)\}")

        def repl(match: re.Match[str]) -> str:
            name = match.group(1) or match.group(2)
            return self.variables.get(name, match.group(0))

        previous = None
        while previous != value:
            previous = value
            value = pattern.sub(repl, value)
        return value

    def resolve_path(self, value: str, base: Path | None = None) -> Path | None:
        expanded = self.expand(value).strip().strip('"\'')
        words = expanded.split()
        if len(words) != 1 or not is_plain_path(expanded):
            return None
        path = Path(expanded)
        if not path.is_absolute():
            path = (base or self.sdk_path) / path
        return path.resolve()


def is_plain_path(value: str) -> bool:
    return not any(token in value for token in ("$", "*", "?", "[", "]", "`", "|", ";")) and not value.startswith("$(shell")


def resolve_assignment_path(cfg: MakeConfig, source: Path, raw_entry: str) -> Path:
    expanded = cfg.expand(raw_entry).strip().strip("\"'")
    path = Path(expanded)
    if path.is_absolute():
        return path.resolve()
    if "$(SDK_PATH)" in raw_entry or "${SDK_PATH}" in raw_entry:
        return (cfg.sdk_path / path).resolve()
    if "$(PROJECT_PATH)" in raw_entry or "${PROJECT_PATH}" in raw_entry:
        return path.resolve()
    if source.resolve() == cfg.config_mk.resolve():
        return (cfg.sdk_path / path).resolve()
    return (source.parent / path).resolve()


def parse_int(value: Any) -> int | None:
    if isinstance(value, int):
        return value
    if not isinstance(value, str):
        return None
    text = value.replace("_", "").strip()
    try:
        return int(text, 0)
    except ValueError:
        return None


def parse_range(value: Any) -> int | None:
    if isinstance(value, int):
        return value
    if not isinstance(value, str):
        return None
    text = value.replace("_", "").strip()
    match = re.fullmatch(r"([0-9]+)([KMGkmg]?)", text)
    if match:
        number = int(match.group(1))
        suffix = match.group(2).upper()
        return number * {"": 1, "K": 1024, "M": 1024**2, "G": 1024**3}[suffix]
    try:
        return int(text, 0)
    except ValueError:
        return None


def validate_config(reporter: Reporter, cfg: MakeConfig) -> None:
    reporter.ok(f"CFG exists: {cfg.config_mk}")
    for name in ("NAME", "VERSION", "BOARD_PATH"):
        if cfg.variables.get(name, "").strip():
            reporter.ok(f"{name} is defined")
        else:
            reporter.err(f"{name} is missing or empty")

    board_path_value = cfg.variables.get("BOARD_PATH", "")
    if board_path_value:
        board_path = Path(board_path_value)
        if not board_path.is_absolute():
            board_path = cfg.sdk_path / board_path
        board_mk = board_path / "board.mk"
        if board_mk.exists():
            reporter.ok(f"BOARD_PATH/board.mk exists: {board_mk}")
        else:
            reporter.err(f"BOARD_PATH/board.mk missing: {board_mk}")

    for source, include_expr in cfg.includes:
        include_path = cfg.resolve_path(include_expr, source.parent)
        if include_path:
            if include_path.exists():
                reporter.ok(f"include exists: {include_path}")
            else:
                reporter.err(f"include missing: {include_expr} from {source}")

    for var in ("DRIVERS", "XDC"):
        for source, value in cfg.assignments.get(var, []):
            for raw_entry in value.split():
                expanded_entry = cfg.expand(raw_entry).strip().strip("\"'")
                if not is_plain_path(expanded_entry):
                    continue
                path = resolve_assignment_path(cfg, source, raw_entry)
                if path.exists():
                    reporter.ok(f"{var} file exists: {path}")
                else:
                    reporter.err(f"{var} file missing: {expanded_entry} from {source}")


def validate_memory(reporter: Reporter, memory_yml: Path) -> None:
    if not memory_yml.exists():
        reporter.err(f"memory.yml missing next to config.mk: {memory_yml}")
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
    parser.add_argument("--cfg", required=True, help="Path to an instrument config.mk")
    parser.add_argument("--no-color", action="store_true", help="Disable colored status labels")
    args = parser.parse_args(argv)

    color = sys.stdout.isatty() and not args.no_color and os.environ.get("NO_COLOR") is None
    reporter = Reporter(color=color)
    sdk_path = Path(args.sdk_path).resolve()
    config_mk = Path(args.cfg)
    if not config_mk.is_absolute():
        config_mk = sdk_path / config_mk
    config_mk = config_mk.resolve()
    if not config_mk.exists():
        reporter.err(f"CFG does not exist: {config_mk}")
        return 1

    cfg = MakeConfig(sdk_path, config_mk)
    cfg.parse()
    validate_config(reporter, cfg)
    validate_memory(reporter, config_mk.parent / "memory.yml")
    return 1 if reporter.errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
