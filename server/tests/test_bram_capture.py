"""Production BRAM driver: fresh captures, manual requests and missing completion."""
import unittest
from cpp_fixture import ROOT, compile_cpp, run_cpp
class BramCaptureTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.binary = compile_cpp(cls, 'server/tests/bram_capture.cpp',
            flags=('-pthread', '-fno-exceptions'),
            includes=(ROOT / 'server/tests/fixtures/bram_capture',))
    def test_cases(self):
        for case in (
            'fresh', 'manual', 'timeout',
        ):
            with self.subTest(case=case):
                run_cpp(self, self.binary, case, timeout=5)
if __name__=='__main__':unittest.main()
