"""Validate FFT full-scale units against independently generated spectra."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]

class FftSpectrumTest(unittest.TestCase):
    def test_calibration_validation_and_full_scale_normalization(self):
        with tempfile.TemporaryDirectory() as work:
            binary = Path(work) / 'fft-spectrum'
            subprocess.run([os.environ.get('CXX', 'g++'), '-std=c++23', '-O2',
                            '-Wall', '-Wextra', '-Werror', '-Wfloat-equal', '-I', str(ROOT),
                            str(ROOT/'server/tests/fft_spectrum.cpp'), '-o', str(binary)],
                           check=True, timeout=60)
            subprocess.run([str(binary)], check=True, timeout=10)
