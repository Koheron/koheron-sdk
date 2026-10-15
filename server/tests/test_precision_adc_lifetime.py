"""Precision ADC worker ownership with a simulated SPI device."""
import unittest
from cpp_fixture import compile_cpp, run_cpp


class PrecisionAdcLifetimeTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.binary = compile_cpp(cls, 'server/tests/precision_adc_lifetime.cpp',
            'boards/alpha250/drivers/precision-adc.cpp',
            flags=('-fno-exceptions', '-pthread'))

    def test_cases(self):
        for case in (
            'inflight', 'setup-errors', 'init-error', 'immediate', 'unavailable', 'errors',
            'samples', 'readers', 'reset-error', 'id-error', 'invalid-id', 'id-revisions',
        ):
            with self.subTest(case=case):
                run_cpp(self, self.binary, case)


if __name__ == '__main__':
    unittest.main()
