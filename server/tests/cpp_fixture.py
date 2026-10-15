"""Compile and run the C++ fixtures used by the unittest suites."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
NETWORK_INCLUDES = (ROOT / 'server/external_libs',
                    os.environ.get('EIGEN_INCLUDE_DIR', '/usr/include/eigen3'))
WEBSOCKET_SOURCES = tuple(ROOT / 'server/network' / name
                          for name in ('websocket.cpp', 'sha1.cpp', 'base64.cpp'))


def compile_cpp(test, *sources, flags=(), includes=(), output=None):
    if output is None:
        directory = tempfile.TemporaryDirectory()
        test.addClassCleanup(directory.cleanup)
        output = Path(directory.name) / 'fixture'
    command = [os.environ.get('CXX', 'g++'), '-std=c++23', '-O2',
               '-Wall', '-Wextra', '-Werror']
    for path in (*includes, ROOT):
        command += ['-I', str(path)]
    command += shlex.split(os.environ.get('CXXFLAGS', ''))
    subprocess.run([*command, *flags, *(str(ROOT / source) for source in sources),
                    '-o', str(output)], check=True, timeout=120)
    return output


def run_cpp(test, binary, *args, timeout=10, cwd=None, returncode=0):
    result = subprocess.run([str(binary), *map(str, args)], capture_output=True,
                            text=True, timeout=timeout, cwd=cwd)
    test.assertEqual(result.returncode, returncode, result.stdout + result.stderr)
    return result
