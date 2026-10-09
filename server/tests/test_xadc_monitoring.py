"""XADC conversion and ALPHA250 temperature history using fake registers."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]

class XadcMonitoringTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.directory.cleanup)
        cls.binary = Path(cls.directory.name) / 'xadc-test'
        command = [os.environ.get('CXX', 'g++'), '-std=c++23', '-O2', '-Wall',
                   '-Wextra', '-Werror', '-fno-exceptions',
                   '-I', str(ROOT / 'server/tests/fixtures/xadc'), '-I', str(ROOT)]
        command += shlex.split(os.environ.get('CXXFLAGS', ''))
        command += [str(ROOT / 'server/tests/xadc_monitoring.cpp'), '-o', str(cls.binary)]
        subprocess.run(command, check=True, timeout=90)

    def run_case(self, case):
        result = subprocess.run([str(self.binary), case], capture_output=True,
                                text=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_first_sample_after_default_and_value_initialization(self):
        self.run_case('startup')

    def test_average_uses_only_acquired_samples(self):
        self.run_case('warmup')

    def test_hundred_sample_window_and_wrap(self):
        self.run_case('window')

    def test_long_running_average_and_full_scale(self):
        self.run_case('drift')

    def test_instances_keep_separate_histories(self):
        self.run_case('instances')

    def test_xadc_temperature_and_all_six_supply_rails(self):
        self.run_case('xadc')

if __name__ == '__main__':
    unittest.main()
