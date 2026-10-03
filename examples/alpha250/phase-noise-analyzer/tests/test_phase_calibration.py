"""Host regression for the C++ phase conversion; no board required."""

import math
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest


class PhaseCalibrationTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        instrument = Path(__file__).resolve().parents[1]
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "calibration.cpp"
            executable = Path(directory) / "calibration"
            source.write_text('''
#include "phase_calibration.hpp"
#include <iomanip>
#include <iostream>

int main() {
    std::cout << std::setprecision(17);
    for (unsigned rate = 4; rate <= 8192; ++rate) {
        std::cout << rate << ' '
                  << phase_calibration::filter_correction(rate, 6, 1) << '\\n';
    }
}
''')
            compiler = shlex.split(os.environ.get("CXX", "g++"))
            subprocess.run(compiler + [
                "-std=c++20", "-Wall", "-Wextra", "-Werror", "-Wpedantic",
                "-I", str(instrument), "-I", str(instrument.parents[2]), str(source), "-o", str(executable),
            ], check=True)
            output = subprocess.check_output([str(executable)], text=True)
        cls.corrections = {
            int(rate): float(correction)
            for rate, correction in (line.split() for line in output.splitlines())
        }

    def test_every_supported_rate_against_exact_integer_gain(self):
        self.assertEqual(len(self.corrections), 8192 - 4 + 1)
        for rate, correction in self.corrections.items():
            # Python integers retain all 79 bits at the maximum CIC rate.
            gain = rate**6
            shift = (gain - 1).bit_length()
            expected = 4 * (1 << shift) / gain
            with self.subTest(rate=rate):
                self.assertTrue(math.isclose(correction, expected, rel_tol=2e-15))
                self.assertGreaterEqual(correction, 4.0)
                self.assertLess(correction, 8.0)

    def test_default_and_power_of_two_rates(self):
        self.assertAlmostEqual(self.corrections[20], 4.194304, places=12)
        for exponent in range(2, 14):
            self.assertEqual(self.corrections[1 << exponent], 4.0)

    def test_shift_boundaries_and_rate_doubling(self):
        # These adjacent rates cross a CIC truncation-bit boundary.
        self.assertGreater(self.corrections[18], self.corrections[17])
        self.assertGreater(self.corrections[21], self.corrections[20])
        for rate in range(4, 4097):
            self.assertEqual(self.corrections[rate], self.corrections[2 * rate])


if __name__ == "__main__":
    unittest.main()
