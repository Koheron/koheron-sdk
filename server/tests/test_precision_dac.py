"""Precision DAC input and calibration failure handling with simulated hardware."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]

class PrecisionDacTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.directory.cleanup)
        cls.root = Path(cls.directory.name)
        for board in ('alpha250', 'alpha15'):
            command = [os.environ.get('CXX', 'g++'), '-std=c++23', '-O2', '-Wall',
                       '-Wextra', '-Werror', '-fno-exceptions', '-pthread',
                       '-I', str(ROOT / 'server/tests/fixtures/precision_dac'), '-I', str(ROOT)]
            command += shlex.split(os.environ.get('CXXFLAGS', ''))
            command += [f'-DDAC_HEADER="boards/{board}/drivers/precision-dac.hpp"',
                        f'-DEEPROM_HEADER="boards/{board}/drivers/eeprom.hpp"',
                        str(ROOT / 'server/tests/precision_dac.cpp'),
                        str(ROOT / f'boards/{board}/drivers/precision-dac.cpp'), '-o', str(cls.root / board)]
            subprocess.run(command, check=True, timeout=90)

    def run_case(self, case):
        for board in ('alpha250', 'alpha15'):
            with self.subTest(board=board):
                result = subprocess.run([str(self.root / board), case], capture_output=True,
                                        text=True, timeout=10)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_raw_bounds(self): self.run_case('raw_bounds')
    def test_only_affected_register_pair_is_written(self): self.run_case('raw_pairs')
    def test_normal_voltage_conversion_and_limits(self): self.run_case('voltage')
    def test_nonfinite_voltage_is_rejected(self): self.run_case('nonfinite')
    def test_calibrated_codes_saturate_without_wrapping(self): self.run_case('clipping')
    def test_every_half_code_rounding_boundary(self): self.run_case("rounding")
    def test_calibration_overflow_does_not_change_output(self): self.run_case('overflow')
    def test_failed_save_keeps_active_calibration(self): self.run_case('save_failure')
    def test_nonfinite_calibration_is_rejected(self): self.run_case('bad_calibration')
    def test_partial_reload_keeps_active_calibration(self): self.run_case('reload_failure')
    def test_missing_calibration_blocks_voltage_commands_until_recovery(self): self.run_case('missing_calibration')

if __name__ == '__main__':
    unittest.main()
