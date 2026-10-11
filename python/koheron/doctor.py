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

if __package__:
    from .diagnostics import GENERATOR_MODULES, HOST_TEST_MODULES, Reporter
    from .sdk_info import load_settings, read_settings
else:
    from diagnostics import GENERATOR_MODULES, HOST_TEST_MODULES, Reporter
    from sdk_info import load_settings, read_settings


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


def check_python(reporter: Reporter, sdk_path: Path, python='python3', modules=False) -> None:
    python3 = shutil.which(python)
    if python3:
        code, version = command_output([python3, "--version"])
        if code == 0:
            reporter.ok(f"Python available: {version} ({python3})")
        else:
            reporter.err(f'Cannot run configured Python: {python3}', 'Run make python_requirements or set HOST_PYTHON.')
    else:
        reporter.err(f'Configured Python executable not found: {python}', 'Run make python_requirements or set HOST_PYTHON.')

    venv = sdk_path / ".venv"
    if venv.exists():
        reporter.ok(".venv exists")
    else:
        reporter.warn(".venv is missing; run make setup or make python_requirements")
    if modules and python3:
        code, _ = command_output([python3, '-c', 'import ' + ', '.join(GENERATOR_MODULES)])
        if code:
            reporter.err('Python generator dependencies are unavailable', 'Run make python_requirements.')
        else:
            reporter.ok('Python generator dependencies available')


def check_xilinx(reporter: Reporter, vivado: Path, vitis: Path, boot=False) -> None:
    required = [vivado / 'settings64.sh', vivado / 'bin/vivado']
    if boot:
        required += [vivado / 'bin/bootgen', vivado / 'bin/xsdb']
    for path in required:
        if path.exists():
            reporter.ok(f"Found {path}")
        else:
            reporter.err(f"Missing {path}", 'Install Xilinx tools separately or set VIVADO_PATH; make setup does not install them.')
    if boot:
        if vitis.exists():
            reporter.ok(f"Found {vitis}")
        else:
            reporter.err(f"Missing {vitis}", 'Install Vitis separately or set VITIS_PATH.')


def check_docker(reporter: Reporter, image=None, web=False) -> None:
    try:
        import grp
    except ImportError:
        grp = None

    docker = shutil.which("docker")
    if docker:
        reporter.ok(f"docker executable found: {docker}")
    else:
        reporter.err("docker executable not found", 'Run make setup to install Docker and the SDK builders.')
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
        reporter.err(f"Docker daemon is not reachable: {detail}", 'Start Docker and check access with docker info.')
        return
    if image:
        code, _ = command_output([docker, 'image', 'inspect', image])
        if code == 0:
            reporter.ok(f'Docker builder available: {image}')
        elif web:
            reporter.warn(f'Web builder missing: {image}; make web builds it automatically.')
        else:
            reporter.err(f'Docker builder missing: {image}', 'Run make setup or select an existing DOCKER_IMAGE; see docker/README.md.')


def check_cfg(reporter: Reporter, values) -> None:
    if not values:
        reporter.ok("CFG not provided; skipping instrument checks")
        return

    config_mk = Path(values['CONFIG_MK'])
    if config_mk.exists():
        reporter.ok(f"CFG config.mk exists: {config_mk}")
    else:
        reporter.err(f"CFG config.mk missing: {config_mk}")
        return

    memory_yml = Path(values['MEMORY_YML'])
    if memory_yml.exists():
        reporter.ok(f"memory.yml exists: {memory_yml}")
    else:
        reporter.err(f"Selected memory.yml missing: {memory_yml}", 'Restore the file or set MEMORY_YML to a valid memory map.')

    for name in ('NAME', 'VERSION', 'BOARD'):
        if values.get(name):
            reporter.ok(f"{name} is defined")
        else:
            reporter.err(f"{name} is not defined in {config_mk}")

    if values.get('BOARD_MK'):
        board_mk = Path(values['BOARD_MK'])
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
    parser.add_argument('--python', default=sys.executable)
    parser.add_argument('--docker-image')
    parser.add_argument('--web-image')
    parser.add_argument('--target', choices=('all', 'web', 'server', 'fpga', 'image', 'os', 'uboot', 'api', 'check'), default='all')
    parser.add_argument('--format', choices=('text', 'json'), default='text')
    parser.add_argument('--settings-stdin', action='store_true')
    parser.add_argument("--no-color", action="store_true", help="Disable colored status labels")
    args = parser.parse_args(argv)

    reporter = Reporter(color=sys.stdout.isatty() and not args.no_color, format=args.format)
    sdk_path = Path(args.sdk_path).resolve()

    check_layout(reporter, sdk_path)
    values = {}
    try:
        if args.settings_stdin:
            values = read_settings(sys.stdin)
        elif args.cfg:
            overrides = [f'VIVADO_VERSION={args.xilinx_version}']
            if args.vivado_path:
                overrides.append(f'VIVADO_PATH={args.vivado_path}')
            if args.vitis_path:
                overrides.append(f'VITIS_PATH={args.vitis_path}')
            values = load_settings(sdk_path, args.cfg, overrides)
    except (OSError, ValueError, subprocess.TimeoutExpired) as error:
        reporter.err(f'Cannot resolve configuration: {error}')
        return reporter.finish(target=args.target)
    check_host_os(reporter)
    if args.target in ('all', 'server', 'fpga', 'image', 'os', 'check'):
        check_python(reporter, sdk_path, python=args.python, modules=args.target != 'check')
    if args.target in ('all', 'fpga', 'image', 'os'):
        xilinx_root = Path("/tools/Xilinx") / args.xilinx_version
        vivado_path = Path(args.vivado_path or values.get('VIVADO_PATH') or xilinx_root / 'Vivado').resolve()
        vitis_path = Path(args.vitis_path or values.get('VITIS_PATH') or xilinx_root / 'Vitis').resolve()
        check_xilinx(reporter, vivado_path, vitis_path, boot=args.target != 'fpga')
    if args.target in ('all', 'server', 'image', 'os', 'uboot', 'api', 'web'):
        if args.target == 'web':
            image = args.web_image or values.get('WEB_DOCKER_IMAGE') or 'koheron-web:node24'
        else:
            image = args.docker_image or values.get('DOCKER_IMAGE') or 'cross-armhf:26.04'
        check_docker(reporter, image=image, web=args.target == 'web')
    check_cfg(reporter, values)
    if args.target == 'check':
        code, _ = command_output([args.python, '-c', 'import ' + ', '.join(HOST_TEST_MODULES)])
        if code:
            reporter.err('Host test dependencies are unavailable', 'Run make python_requirements and make koheron_python.')
        else:
            reporter.ok('Host test dependencies available')

    return reporter.finish(target=args.target, settings=values)


if __name__ == "__main__":
    raise SystemExit(main())
