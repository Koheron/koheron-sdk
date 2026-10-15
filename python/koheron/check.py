"""Run configuration and host regressions without board or Xilinx tools."""
import argparse
import importlib.util
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time

if __package__:
    from .diagnostics import HOST_TEST_MODULES, Reporter
    from .sdk_info import read_settings
    from .list_examples import find_configs
else:
    from diagnostics import HOST_TEST_MODULES, Reporter
    from sdk_info import read_settings
    from list_examples import find_configs


SUITES = (('python/tests', 'test_*.py'), ('make/tests', 'test_*.py'),
          ('os/tests', 'test_*_make.py'),
          ('fpga/tests/build_flow', 'test_*.py'),
          ('server/tests', 'test_build_make.py'), ('web/tests', 'test_build.py'))


def run_check(reporter, label, command, sdk_path):
    started = time.monotonic()
    # Fixture commands choose their own board, configuration and run options.
    env = {**os.environ, 'PYTHONPATH': str(sdk_path / 'python')}
    for key in ('CFG', 'BOARD_PATH', 'VIVADO_VERSION', 'MEMORY_YML', 'HOST', 'TIMEOUT', 'LOG_LIMIT', 'FORMAT',
                'TARGET', 'ENFORCE_TIMING', 'MAKEFLAGS', 'MFLAGS', 'MAKEOVERRIDES'):
        env.pop(key, None)
    try:
        result = subprocess.run(command, cwd=sdk_path, env=env, text=True,
                                capture_output=True, timeout=120)
        output = (result.stdout + result.stderr).strip()
        if result.returncode:
            record = reporter.err(label)
            record['output'] = output
            if reporter.format == 'text':
                print(output, file=sys.stderr)
        else:
            record = reporter.ok(label)
        summary = re.search(r'^Ran (\d+) tests? in ', output, re.MULTILINE)
        if summary:
            record['tests'] = int(summary.group(1))
    except (OSError, subprocess.TimeoutExpired) as error:
        record = reporter.err(f'{label}: {error}')
    record['seconds'] = round(time.monotonic() - started, 3)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk-path', type=Path, default=Path('.'))
    parser.add_argument('--format', choices=('text', 'json'), default='text')
    parser.add_argument('--settings-stdin', action='store_true')
    args = parser.parse_args(argv)
    sdk_path = args.sdk_path.resolve()
    reporter = Reporter(color=sys.stdout.isatty(), format=args.format)
    for module in HOST_TEST_MODULES:
        if importlib.util.find_spec(module) is None:
            reporter.err(f'Missing host test dependency: {module}', 'Run make python_requirements and make koheron_python.')
    if not shutil.which('tclsh'):
        reporter.err('Missing host test dependency: tclsh', 'Install Tcl or run make setup.')
    if reporter.errors:
        return reporter.finish(scope='host', hardware_tests=False, configurations=0, tests=0)
    if args.settings_stdin:
        try:
            values = read_settings(sys.stdin)
        except ValueError as error:
            reporter.err(f'Cannot read resolved configuration: {error}')
            return reporter.finish(scope='host', hardware_tests=False, configurations=0, tests=0)
        configs = [(values['CONFIG_MK'], values['MEMORY_YML'])]
    else:
        configs = [(str(path), None) for path in find_configs(sdk_path)]
        if not configs:
            reporter.err('No example configurations found')
    validator = sdk_path / 'python/koheron/validate_config.py'
    for cfg, memory in configs:
        command = [sys.executable, str(validator), '--sdk-path', str(sdk_path), '--cfg', cfg, '--no-color']
        if memory:
            command += ['--memory-yml', memory]
        run_check(reporter, f'Configuration: {cfg}', command, sdk_path)
    for directory, pattern in SUITES:
        run_check(reporter, f'Host tests: {directory} ({pattern})',
                  [sys.executable, '-m', 'unittest', 'discover', '-s', directory, '-p', pattern], sdk_path)
    tests = sum(record.get('tests', 0) for record in reporter.checks)
    if args.format == 'text':
        print(f'Configurations: {len(configs)}; host tests: {tests}.')
    return reporter.finish(scope='host', hardware_tests=False, configurations=len(configs), tests=tests)


if __name__ == '__main__':
    raise SystemExit(main())
