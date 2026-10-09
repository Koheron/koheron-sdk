"""SPI state and transfer contracts, with kernel calls captured by a fixture."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]

class SpiTransfersTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.directory.cleanup)
        cls.binary = Path(cls.directory.name) / 'spi-test'
        command = [os.environ.get('CXX', 'g++'), '-std=c++23', '-O2', '-Wall',
                   '-Wextra', '-Werror', '-fno-exceptions', '-DNDEBUG', '-I', str(ROOT)]
        command += shlex.split(os.environ.get('CXXFLAGS', ''))
        command += [str(ROOT / 'server/tests/spi_transfers.cpp'),
                    str(ROOT / 'server/hardware/spi_manager.cpp'),
                    '-Wl,--wrap=open', '-Wl,--wrap=open64', '-Wl,--wrap=ioctl',
                    '-Wl,--wrap=__ioctl_time64',
                    '-o', str(cls.binary)]
        subprocess.run(command, check=True, timeout=90)

    def run_case(self, case):
        result = subprocess.run([str(self.binary), case], capture_output=True,
                                text=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_empty_spans_have_null_kernel_buffers(self):
        self.run_case('empty')

    def test_failed_settings_preserve_transfer_settings(self):
        self.run_case('settings')

    def test_initialization_failure_closes_and_retries(self):
        self.run_case('init')

    def test_array_bounds_checked_in_release_builds(self):
        self.run_case('bounds')

    def test_normal_transfers_and_error_propagation(self):
        self.run_case('normal')

    def test_oversized_raw_requests_are_rejected(self):
        self.run_case('length')

if __name__ == '__main__':
    unittest.main()
