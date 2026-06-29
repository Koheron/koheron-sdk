#!/usr/bin/env python3
"""Check the local Koheron SDK development environment."""

from __future__ import annotations

import argparse
import os
import platform
import re
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path


GREEN = "\033[1;32m"
YELLOW = "\033[1;33m"
RED = "\033[1;31m"
BLUE = "\033[1;34m"
RESET = "\033[0m"


@dataclass
class Result:
    level: str
    message: str
    detail: str | None = None


class Doctor:
    def __init__(self) -> None:
        self.results: list[Result] = []

    def ok(self, message: str, detail: str | None = None) -> None:
        self.results.append(Result("OK", message, detail))

    def warn(self, message: str, detail: str | None = None) -> None:
        self.results.append(Result("WARN", message, detail))

    def err(self, message: str, detail: str | None = None) -> None:
        self.results.append(Result("ERR", message, detail))

    def print(self) -> None:
        colors = {"OK": GREEN, "WARN": YELLOW, "ERR": RED}
        for result in self.results:
            color = colors[result.level]
            print(f"{color}[{result.level}]{RESET} {result.message}")
            if result.detail:
                print(f"     {result.detail}")

    def has_errors(self) -> bool:
        return any(result.level == "ERR" for result in self.results)


def _read_os_release() -> dict[str, str]:
    path = Path("/etc/os-release")
    if not path.exists():
        return {}

    values: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, value = line.split("=", 1)
        values[key] = value.strip().strip('"')
    return values


def _run(command: list[str], timeout: float = 5.0) -> tuple[int, str]:
    try:
        completed = subprocess.run(
            command,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            timeout=timeout,
            check=False,
        )
    except FileNotFoundError:
        return 127, "command not found"
    except subprocess.TimeoutExpired:
        return 124, "command timed out"

    return completed.returncode, completed.stdout.strip()


def _resolve_path(value: str, sdk_path: Path) -> Path:
    value = value.replace("$(SDK_PATH)", str(sdk_path))
    value = value.replace("${SDK_PATH}", str(sdk_path))
    return Path(value).expanduser()


def _parse_make_assignment(path: Path, name: str) -> str | None:
    pattern = re.compile(rf"^\s*{re.escape(name)}\s*(?::=|=|\+=)\s*(.*?)\s*$")
    try:
        lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    except OSError:
        return None

    for line in lines:
        line = line.split("#", 1)[0].strip()
        match = pattern.match(line)
        if match:
            return match.group(1).strip()
    return None


def check_repository(doctor: Doctor, sdk_path: Path) -> None:
    expected = ["Makefile", "README.md", "requirements.txt", "python", "fpga", "os", "server", "web"]
    missing = [entry for entry in expected if not (sdk_path / entry).exists()]
    if missing:
        doctor.err("SDK repository layout is incomplete", "missing: " + ", ".join(missing))
    else:
        doctor.ok("SDK repository layout looks complete", str(sdk_path))


def check_python(doctor: Doctor, venv: Path) -> None:
    version = sys.version_info
    if version.major == 3:
        doctor.ok("Python 3 is available", platform.python_version())
    else:
        doctor.err("Python 3 is required", platform.python_version())

    if venv.exists():
        python_bin = venv / "bin" / "python3"
        if python_bin.exists():
            doctor.ok("Python virtual environment exists", str(venv))
        else:
            doctor.warn("Python virtual environment exists but bin/python3 is missing", str(venv))
    else:
        doctor.warn("Python virtual environment is missing", f"run: make setup")


def check_os(doctor: Doctor) -> None:
    os_release = _read_os_release()
    pretty = os_release.get("PRETTY_NAME") or platform.platform()
    ubuntu_version = os_release.get("VERSION_ID") if os_release.get("ID") == "ubuntu" else None

    if ubuntu_version == "24.04":
        doctor.ok("Host OS matches the tested Ubuntu release", pretty)
    elif ubuntu_version:
        doctor.warn("Host OS is Ubuntu but not the tested 24.04 release", pretty)
    else:
        doctor.warn("Host OS is not Ubuntu 24.04", pretty)


def check_xilinx(doctor: Doctor, vivado_version: str, vivado_path: Path, vitis_path: Path) -> None:
    settings64 = vivado_path / "settings64.sh"
    if settings64.exists():
        doctor.ok(f"Vivado {vivado_version} settings file found", str(settings64))
    else:
        doctor.err(f"Vivado {vivado_version} settings file not found", str(settings64))

    if vitis_path.exists():
        doctor.ok(f"Vitis {vivado_version} path found", str(vitis_path))
    else:
        doctor.warn(f"Vitis {vivado_version} path not found", str(vitis_path))

    for tool in ("vivado", "bootgen"):
        executable = vivado_path / "bin" / tool
        if executable.exists():
            doctor.ok(f"{tool} executable found", str(executable))
        else:
            doctor.warn(f"{tool} executable not found before sourcing settings64.sh", str(executable))

    xsdb = vivado_path / "bin" / "xsdb"
    if xsdb.exists():
        doctor.ok("xsdb executable found", str(xsdb))
    else:
        doctor.warn("xsdb executable not found", str(xsdb))


def check_docker(doctor: Doctor) -> None:
    docker = shutil.which("docker")
    if not docker:
        doctor.err("Docker executable not found", "run: make setup")
        return

    doctor.ok("Docker executable found", docker)

    groups = set(os.getgroups())
    in_docker_group = False
    try:
        import grp

        docker_group = grp.getgrnam("docker")
        in_docker_group = docker_group.gr_gid in groups
    except (KeyError, OSError):
        pass

    if in_docker_group or os.geteuid() == 0:
        doctor.ok("Current user can likely access the docker group")
    else:
        doctor.warn("Current user is not in the docker group", "log out/in after make setup, or use sudo for docker")

    code, output = _run([docker, "info", "--format", "{{.ServerVersion}}"], timeout=3.0)
    if code == 0 and output:
        doctor.ok("Docker daemon is reachable", output)
    else:
        doctor.warn("Docker daemon is not reachable", output or "is the docker service running?")


def check_cfg(doctor: Doctor, sdk_path: Path, cfg: str | None) -> None:
    if not cfg:
        doctor.warn("No CFG was provided", "run with: make doctor CFG=examples/<board>/<instrument>/config.mk")
        return

    cfg_path = _resolve_path(cfg, sdk_path)
    if not cfg_path.is_absolute():
        cfg_path = sdk_path / cfg_path
    cfg_path = cfg_path.resolve()

    if not cfg_path.exists():
        doctor.err("CFG does not reference an existing config.mk", str(cfg_path))
        return

    doctor.ok("CFG file found", str(cfg_path))

    project_path = cfg_path.parent
    memory_yml = project_path / "memory.yml"
    if memory_yml.exists():
        doctor.ok("memory.yml found", str(memory_yml))
    else:
        doctor.err("memory.yml not found next to config.mk", str(memory_yml))

    name = _parse_make_assignment(cfg_path, "NAME")
    version = _parse_make_assignment(cfg_path, "VERSION")
    board_path_value = _parse_make_assignment(cfg_path, "BOARD_PATH")

    if name:
        doctor.ok("Instrument name is defined", name)
    else:
        doctor.err("NAME is not defined in config.mk")

    if version:
        doctor.ok("Instrument version is defined", version)
    else:
        doctor.warn("VERSION is not defined in config.mk")

    if board_path_value:
        board_path = _resolve_path(board_path_value, sdk_path)
        if not board_path.is_absolute():
            board_path = (cfg_path.parent / board_path).resolve()
        board_mk = board_path / "board.mk"
        if board_mk.exists():
            doctor.ok("Board definition found", str(board_mk))
        else:
            doctor.err("BOARD_PATH does not contain board.mk", str(board_mk))
    else:
        doctor.err("BOARD_PATH is not defined in config.mk")


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk-path", default=".", help="Path to the SDK root")
    parser.add_argument("--vivado-version", default="2025.1", help="Expected Vivado/Vitis version")
    parser.add_argument("--vivado-path", default="/tools/Xilinx/2025.1/Vivado", help="Path to Vivado")
    parser.add_argument("--vitis-path", default="/tools/Xilinx/2025.1/Vitis", help="Path to Vitis")
    parser.add_argument("--venv", default=".venv", help="Path to the Python virtual environment")
    parser.add_argument("--cfg", default=None, help="Optional instrument config.mk to check")
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    sdk_path = Path(args.sdk_path).expanduser().resolve()
    venv = _resolve_path(args.venv, sdk_path)
    if not venv.is_absolute():
        venv = sdk_path / venv

    doctor = Doctor()
    print(f"{BLUE}Koheron SDK doctor{RESET}\n")

    check_repository(doctor, sdk_path)
    check_os(doctor)
    check_python(doctor, venv)
    check_xilinx(
        doctor,
        args.vivado_version,
        _resolve_path(args.vivado_path, sdk_path),
        _resolve_path(args.vitis_path, sdk_path),
    )
    check_docker(doctor)
    check_cfg(doctor, sdk_path, args.cfg)

    print()
    doctor.print()

    if doctor.has_errors():
        print(f"\n{RED}Doctor found errors that should be fixed before building.{RESET}")
        return 1

    print(f"\n{GREEN}Doctor completed without errors.{RESET}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
