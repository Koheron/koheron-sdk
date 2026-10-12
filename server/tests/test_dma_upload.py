"""DMA upload bounds and waveform order using the production ALPHA250 driver."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class DmaUploadTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        directory = tempfile.TemporaryDirectory()
        cls.addClassCleanup(directory.cleanup)
        cls.binary = Path(directory.name) / 'dma-upload'
        command = [os.environ.get('CXX', 'g++'), '-std=c++23', '-O2', '-Wall',
                   '-Wextra', '-Werror', '-pthread',
                   '-I', str(ROOT / 'server/tests/fixtures/dma_upload'), '-I', str(ROOT)]
        command += shlex.split(os.environ.get('CXXFLAGS', ''))
        command += [str(ROOT / 'server/tests/dma_upload.cpp'), '-o', str(cls.binary)]
        subprocess.run(command, check=True, timeout=90)

    def run_case(self, case):
        result = subprocess.run([str(self.binary), case], capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_two_descriptor_waveform(self): self.run_case('two-descriptors')
    def test_reserved_memory_bounds(self): self.run_case('bounds')
    def test_legacy_upload_compatibility_and_bounds(self): self.run_case('legacy')


if __name__ == '__main__':
    unittest.main()
