"""Commands for the optional compiled, browser and sanitizer host suites."""
from dataclasses import dataclass, field
import os
import shutil
import sys


@dataclass
class Suite:
    label: str
    command: list[str]
    env: dict[str, str] = field(default_factory=dict)
    timeout: int = 120
    backend: str = 'native'


def backend(mode, executable):
    if mode == 'auto':
        mode = 'native' if shutil.which(executable) else 'docker'
    required = executable if mode == 'native' else 'docker'
    if not shutil.which(required):
        raise ValueError(f'{required} is unavailable; select CHECK_MODE=docker or install the native tools')
    return mode


def docker_command(sdk_path, image, command, env=None, mounts=()):
    args = ['docker', 'run', '--rm', '--pull=never', '--init',
            '-u', f'{os.getuid()}:{os.getgid()}', '-v', f'{sdk_path}:{sdk_path}', '-w', str(sdk_path)]
    for path in mounts:
        args += ['-v', f'{path}:{path}']
    for key, value in (env or {}).items():
        args += ['-e', f'{key}={value}']
    return [*args, image, *command]


def fixture_command(sdk_path, directory, command):
    """Compile and execute temporary C++ fixtures in the selected PNA backend."""
    if os.environ.get('PNA_TEST_MODE', 'native') != 'docker':
        return command
    return docker_command(sdk_path, os.environ.get('PNA_CPP_IMAGE', 'cross-armhf:26.04'),
                          command, mounts=(directory,))


def server_suite(sdk_path, mode, cxx, image):
    selected = backend(mode, cxx)
    # Build-rule fixtures already run in the fast profile with host generator
    # dependencies. The remaining server modules compile and run native C++.
    modules = sorted(path.stem for path in (sdk_path / 'server/tests').glob('test_*.py')
                     if path.stem != 'test_build_make')
    if not modules:
        raise ValueError('No server C++ test modules found')
    env = {'CXX': cxx, 'CXXFLAGS': os.environ.get('CXXFLAGS', ''),
           'PYTHONPATH': os.pathsep.join((str(sdk_path / 'python'), str(sdk_path / 'server/tests')))}
    command = [sys.executable if selected == 'native' else 'python3', '-m', 'unittest', *modules]
    if selected == 'docker':
        command = docker_command(sdk_path, image, command, env)
    return Suite('Server C++ tests', command, env, timeout=900, backend=selected)


def pna_suites(sdk_path, mode, cxx, cpp_image, web_image):
    selected = backend(mode, cxx)
    env = {'PNA_TEST_MODE': selected, 'PNA_TEST_CXX': cxx, 'PNA_PYTHON': sys.executable,
           'PNA_CPP_IMAGE': cpp_image, 'PNA_WEB_IMAGE': web_image,
           'PNA_TEST_CXXFLAGS': os.environ.get('PNA_TEST_CXXFLAGS', os.environ.get('CXXFLAGS', ''))}
    runner = str(sdk_path / 'server/drivers/phase-noise/tests/run-host.sh')
    return [Suite('PNA/DPLL Python tests', ['bash', runner, 'all', 'python'], env, backend=selected),
            Suite('PNA/DPLL ASan/UBSan and numerical checks', ['bash', runner, 'all', 'cpp'],
                  env, timeout=1800, backend=selected)]


def browser_suite(sdk_path, mode, image):
    selected = backend(mode, 'node')
    command = ['bash', str(sdk_path / 'web/tests/run.sh'), 'all']
    if selected == 'docker':
        command = docker_command(sdk_path, image, command)
    return Suite('Browser tests', command, timeout=300, backend=selected)
