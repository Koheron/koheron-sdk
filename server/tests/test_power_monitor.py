"""INA230 bounds, signed conversion and I2C failures using simulated registers."""
import unittest
from cpp_fixture import compile_cpp, run_cpp


class PowerMonitorTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.binary = compile_cpp(cls, 'server/tests/power_monitor.cpp', 'boards/alpha250/drivers/power-monitor.cpp',
            flags=('-fno-exceptions', '-pthread'))

    def test_cases(self):
        for case in (
            'bus', 'shunt', 'bounds', 'read_failure', 'write_failure', 'ui',
        ):
            with self.subTest(case=case):
                run_cpp(self, self.binary, case)


if __name__ == '__main__':
    unittest.main()
