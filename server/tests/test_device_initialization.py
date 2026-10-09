"""Device failures and move-only IRQ callbacks using wrapped I/O and a raw PTY."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest
from uio_fixture import prepare_uio_fixture
ROOT = Path(__file__).resolve().parents[2]

class DeviceInitializationTest(unittest.TestCase):
    def compile_run(self, sources, includes=(), flags=(), args=()):
        with tempfile.TemporaryDirectory() as directory:
            binary = Path(directory) / 'test'
            command = [os.environ.get('CXX', 'g++'), '-std=c++23', '-O2', '-Wall',
                       '-Wextra', '-Werror', '-fno-exceptions', '-pthread']
            for path in [*includes, ROOT]:
                command += ['-I', str(path)]
            command += shlex.split(os.environ.get('CXXFLAGS', ''))
            command += list(flags) + [str(ROOT / s) for s in sources]
            subprocess.run(command + ['-o', str(binary)], check=True, timeout=90)
            result = subprocess.run([str(binary), *map(str, args)], capture_output=True,
                                    text=True, timeout=10)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            return result.stdout + result.stderr

    def test_i2c_path_lifetime_and_retry(self):
        output = self.compile_run(['server/tests/i2c_initialization.cpp',
                                   'server/hardware/i2c_manager.cpp'], flags=[
            '-Wl,--wrap=open', '-Wl,--wrap=open64', '-Wl,--wrap=opendir',
            '-Wl,--wrap=readdir', '-Wl,--wrap=readdir64', '-Wl,--wrap=closedir'])
        self.assertIn('Permission denied', output)

    def test_move_only_uio_callback_lifetime(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            prepare_uio_fixture(root)
            self.compile_run(['server/tests/uio_callbacks.cpp'], includes=[root], args=[root])

if __name__ == '__main__':
    unittest.main()
