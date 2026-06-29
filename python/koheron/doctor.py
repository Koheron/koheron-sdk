#!/usr/bin/env python3
"""Fast SDK environment diagnostics for Koheron builds."""

from __future__ import annotations

import argparse
import os
import platform
import shutil
import subprocess
import sys
from pathlib import Path


DEFAULT_XILINX_VERSION = "2025.1"
REQUIRED_LAYOUT = (
    "Makefile",
    "README.md",
    "requirements.txt",
    "python",
    "fpga",
    "os",
    "server",
    "web",
)


class Reporter:
    def __init__(self, color: bool) -> None:
        self.color = color
        self.errors = 0

    def _line(self, level: str, message: str) -> None:
        colors = {"OK": "\033[1;32m", "WARN": "\033[1;33m", "ERR": "\033[1;31m"}
        reset = "\033[0m"
        label = f"[{level}]"
        if self.color:
            label = f"{colors[level]}{label}{reset}"
        print(f"{label} {message}")
        if level == "ERR":
            self.errors += 1

    def ok(self, message: str) -> None:
        self._line("OK", message)

    def warn(self, message: str) -> None:
        self._line("WARN", message)

    def err(self, message: str) -> None:
        self._line("ERR", message)


def command_output(command: list[str], timeout: float = 5.0) -> tuple[int, str]:
    try:
        completed = subprocess.run(
            command,
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            timeout=timeout,
        )
    except (OSError, subprocess.TimeoutExpired) as exc:
        return 1, str(exc)
    return completed.returncode, completed.stdout.strip()


def check_layout(reporter: Reporter, sdk_path: Path) -> None:
    if sdk_path.exists():
        reporter.ok(f"SDK path: {sdk_path}")
    else:
        reporter.err(f"SDK path missing: {sdk_path}")
    for item in REQUIRED_LAYOUT:
        path = sdk_path / item
        if path.exists():
            reporter.ok(f"Found {item}")
        else:
            reporter.err(f"Missing {item}")


def os_release() -> dict[str, str]:
    data: dict[str, str] = {}
    try:
        with open("/etc/os-release", encoding="utf-8") as release:
            for line in release:
                if "=" in line:
                    key, value = line.rstrip().split("=", 1)
                    data[key] = value.strip('"')
    except OSError:
        pass
    return data


def check_host_os(reporter: Reporter) -> None:
    release = os_release()
    name = release.get("PRETTY_NAME") or platform.platform()
    if release.get("ID") == "ubuntu" and release.get("VERSION_ID") == "24.04":
        reporter.ok(f"Host OS is Ubuntu 24.04 ({name})")
    else:
        reporter.warn(f"Host OS is {name}; Ubuntu 24.04 is preferred")


def check_python(reporter: Reporter, sdk_path: Path) -> None:
    python3 = shutil.which("python3")
    if python3:
        code, version = command_output([python3, "--version"])
        reporter.ok(f"Python 3 available: {version if code == 0 else python3}")
    else:
        reporter.err("python3 executable not found")

    venv = sdk_path / ".venv"
    if venv.exists():
        reporter.ok(".venv exists")
    else:
        reporter.warn(".venv is missing; run make setup or make python_requirements")


def check_xilinx(reporter: Reporter, vivado: Path, vitis: Path) -> None:
    required = (
        vivado / "settings64.sh",
        vivado / "bin" / "vivado",
        vivado / "bin" / "bootgen",
        vivado / "bin" / "xsdb",
    )
    for path in required:
        if path.exists():
            reporter.ok(f"Found {path}")
        else:
            reporter.err(f"Missing {path}")
    if vitis.exists():
        reporter.ok(f"Found {vitis}")
    else:
        reporter.warn(f"Missing {vitis}; Vitis is recommended")


def check_docker(reporter: Reporter) -> None:
    try:
        import grp
    except ImportError:
        grp = None

    docker = shutil.which("docker")
    if docker:
        reporter.ok(f"docker executable found: {docker}")
    else:
        reporter.err("docker executable not found")
        return

    if os.geteuid() == 0:
        reporter.ok("Running as root; docker group membership is not required")
    elif grp is None:
        reporter.warn("Cannot check docker group membership on this platform")
    else:
        groups = set()
        for gid in os.getgroups():
            try:
                groups.add(grp.getgrgid(gid).gr_name)
            except KeyError:
                continue
        if "docker" in groups:
            reporter.ok("Current user is in the docker group")
        else:
            reporter.warn("Current user is not in the docker group")

    code, output = command_output([docker, "info"], timeout=10.0)
    if code == 0:
        reporter.ok("Docker daemon is reachable")
    else:
        detail = output.splitlines()[-1] if output else "docker info failed"
        reporter.warn(f"Docker daemon is not reachable: {detail}")


def parse_make_vars(config_mk: Path) -> dict[str, str]:
    variables: dict[str, str] = {}
    try:
        for raw_line in config_mk.read_text(encoding="utf-8").splitlines():
            line = raw_line.split("#", 1)[0].strip()
            if not line or "=" not in line:
                continue
            for operator in (":=", "?=", "+=", "="):
                if operator in line:
                    key, value = line.split(operator, 1)
                    variables[key.strip()] = value.strip()
                    break
    except OSError:
        pass
    return variables


def expand_make_path(value: str, sdk_path: Path, project_path: Path) -> str:
    replacements = {
        "$(SDK_PATH)": str(sdk_path),
        "${SDK_PATH}": str(sdk_path),
        "$(PROJECT_PATH)": str(project_path),
        "${PROJECT_PATH}": str(project_path),
    }
    for token, replacement in replacements.items():
        value = value.replace(token, replacement)
    return value


def check_cfg(reporter: Reporter, sdk_path: Path, cfg: str | None) -> None:
    if not cfg:
        reporter.ok("CFG not provided; skipping instrument checks")
        return

    config_mk = Path(cfg)
    if not config_mk.is_absolute():
        config_mk = sdk_path / config_mk
    config_mk = config_mk.resolve()
    if config_mk.exists():
        reporter.ok(f"CFG config.mk exists: {config_mk}")
    else:
        reporter.err(f"CFG config.mk missing: {config_mk}")
        return

    memory_yml = config_mk.parent / "memory.yml"
    if memory_yml.exists():
        reporter.ok(f"memory.yml exists: {memory_yml}")
    else:
        reporter.err(f"memory.yml missing next to config.mk: {memory_yml}")

    variables = parse_make_vars(config_mk)
    for name in ("NAME", "VERSION", "BOARD_PATH"):
        if variables.get(name):
            reporter.ok(f"{name} is defined")
        else:
            reporter.err(f"{name} is not defined in {config_mk}")

    board_path_value = variables.get("BOARD_PATH")
    if board_path_value:
        board_path = Path(expand_make_path(board_path_value, sdk_path, config_mk.parent))
        if not board_path.is_absolute():
            board_path = sdk_path / board_path
        board_mk = board_path / "board.mk"
        if board_mk.exists():
            reporter.ok(f"BOARD_PATH/board.mk exists: {board_mk}")
        else:
            reporter.err(f"BOARD_PATH/board.mk missing: {board_mk}")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Check Koheron SDK host tools and optional CFG before building.")
    parser.add_argument("--sdk-path", default=".", help="Path to the Koheron SDK repository")
    parser.add_argument("--cfg", default=os.environ.get("CFG"), help="Optional instrument config.mk path")
    parser.add_argument("--xilinx-version", default=os.environ.get("VIVADO_VERSION", DEFAULT_XILINX_VERSION))
    parser.add_argument("--vivado-path", help="Path to the Vivado installation")
    parser.add_argument("--vitis-path", help="Path to the Vitis installation")
    parser.add_argument("--no-color", action="store_true", help="Disable colored status labels")
    args = parser.parse_args(argv)

    color = sys.stdout.isatty() and not args.no_color and os.environ.get("NO_COLOR") is None
    reporter = Reporter(color=color)
    sdk_path = Path(args.sdk_path).resolve()

    check_layout(reporter, sdk_path)
    check_host_os(reporter)
    check_python(reporter, sdk_path)
    xilinx_root = Path("/tools/Xilinx") / args.xilinx_version
    vivado_path = Path(args.vivado_path).resolve() if args.vivado_path else xilinx_root / "Vivado"
    vitis_path = Path(args.vitis_path).resolve() if args.vitis_path else xilinx_root / "Vitis"

    check_xilinx(reporter, vivado_path, vitis_path)
    check_docker(reporter)
    check_cfg(reporter, sdk_path, args.cfg)

    return 1 if reporter.errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
