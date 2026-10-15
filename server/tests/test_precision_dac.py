"""Precision DAC input and calibration failure handling with simulated hardware."""
from pathlib import Path
import tempfile
import unittest
from cpp_fixture import ROOT, compile_cpp, run_cpp


class PrecisionDacTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.directory.cleanup)
        cls.root = Path(cls.directory.name)
        for board in ('alpha250', 'alpha15'):
            compile_cpp(cls, 'server/tests/precision_dac.cpp',
                f'boards/{board}/drivers/precision-dac.cpp',
                flags=('-fno-exceptions', '-pthread',
                       f'-DDAC_HEADER="boards/{board}/drivers/precision-dac.hpp"',
                       f'-DEEPROM_HEADER="boards/{board}/drivers/eeprom.hpp"'),
                includes=(ROOT / 'server/tests/fixtures/precision_dac',), output=cls.root / board)

    def test_cases(self):
        for case in (
            'clipping', 'overflow', 'rounding', 'save_failure', 'missing_calibration',
            'bad_calibration', 'nonfinite', 'voltage', 'raw_pairs', 'reload_failure',
            'raw_bounds',
        ):
            with self.subTest(case=case):
                for board in ('alpha250', 'alpha15'):
                    with self.subTest(board=board):
                        run_cpp(self, self.root / board, case)


if __name__ == '__main__':
    unittest.main()
