"""Typed I2C buffers and single-transaction error handling using wrapped I/O."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]

class I2cTransfersTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.directory.cleanup)
        cls.binary = Path(cls.directory.name) / 'i2c-test'
        command = [os.environ.get('CXX', 'g++'), '-std=c++23', '-O2', '-Wall',
                   '-Wextra', '-Werror', '-fno-exceptions', '-pthread', '-I', str(ROOT)]
        command += shlex.split(os.environ.get('CXXFLAGS', ''))
        command += [str(ROOT / 'server/tests/i2c_transfers.cpp'),
                    str(ROOT / 'server/hardware/i2c_manager.cpp')]
        for symbol in ['open', 'open64', 'opendir', 'readdir', 'readdir64',
                       'closedir', 'ioctl', '__ioctl_time64', 'read', 'write']:
            command += ['-Wl,--wrap=' + symbol]
        subprocess.run(command + ['-o', str(cls.binary)], check=True, timeout=90)

    def run_case(self, case):
        result = subprocess.run([str(self.binary), case], capture_output=True,
                                text=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_short_read_is_not_replayed(self):
        self.run_case('short_read')

    def test_short_write_fails_without_replay(self):
        self.run_case('short_write')

    def test_invalid_lengths_and_null_buffers(self):
        self.run_case('bounds')

    def test_address_cache_and_failed_selection(self):
        self.run_case('address')

    def test_typed_buffers_preserve_native_bytes(self):
        self.run_case('typed')

    def test_address_selection_stays_serialized(self):
        self.run_case('concurrent')

    def test_empty_buffers_do_not_transfer(self):
        self.run_case('empty')

if __name__ == '__main__':
    unittest.main()
