"""DMA upload bounds and waveform order using the production ALPHA250 driver."""
import unittest
from cpp_fixture import ROOT, compile_cpp, run_cpp


class DmaUploadTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.binary = compile_cpp(cls, 'server/tests/dma_upload.cpp',
            flags=('-pthread',), includes=(ROOT / 'server/tests/fixtures/dma_upload',))

    def test_cases(self):
        for case in (
            'legacy', 'bounds', 'two-descriptors',
        ):
            with self.subTest(case=case):
                run_cpp(self, self.binary, case)


if __name__ == '__main__':
    unittest.main()
