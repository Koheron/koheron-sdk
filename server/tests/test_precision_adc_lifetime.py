"""Precision ADC worker ownership with a simulated SPI device."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]

class PrecisionAdcLifetimeTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.directory.cleanup)
        cls.binary = Path(cls.directory.name) / 'precision-adc-test'
        command = [os.environ.get('CXX', 'g++'), '-std=c++23', '-O2', '-Wall',
                   '-Wextra', '-Werror', '-fno-exceptions', '-pthread', '-I', str(ROOT)]
        command += shlex.split(os.environ.get('CXXFLAGS', ''))
        command += [str(ROOT / 'server/tests/precision_adc_lifetime.cpp'),
                    str(ROOT / 'boards/alpha250/drivers/precision-adc.cpp'), '-o', str(cls.binary)]
        subprocess.run(command, check=True, timeout=90)

    def run_case(self, case):
        result = subprocess.run([str(self.binary), case], capture_output=True,
                                text=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_missing_spi_does_not_start_worker(self):
        self.run_case('unavailable')

    def test_samples_and_configuration_are_preserved(self):
        self.run_case('samples')

    def test_destruction_joins_in_flight_read(self):
        self.run_case('inflight')

    def test_read_failure_still_allows_shutdown(self):
        self.run_case('errors')

    def test_immediate_repeated_destruction(self):
        self.run_case('immediate')

    def test_snapshots_during_acquisition(self):
        self.run_case('readers')

if __name__ == '__main__':
    unittest.main()
