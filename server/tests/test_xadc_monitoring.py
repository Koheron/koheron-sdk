"""XADC conversion and ALPHA250 temperature history using fake registers."""
import unittest
from cpp_fixture import ROOT, compile_cpp, run_cpp


class XadcMonitoringTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.binary = compile_cpp(cls, 'server/tests/xadc_monitoring.cpp',
            flags=('-fno-exceptions',), includes=(ROOT / 'server/tests/fixtures/xadc',))

    def test_cases(self):
        for case in (
            'warmup', 'startup', 'window', 'instances', 'drift', 'xadc',
        ):
            with self.subTest(case=case):
                run_cpp(self, self.binary, case)


if __name__ == '__main__':
    unittest.main()
