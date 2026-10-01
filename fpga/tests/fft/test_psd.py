"""Alpha250 is the frozen reference; Red Pitaya adapts dimensions and its DSP budget."""
from pathlib import Path
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent


class DatapathTest(unittest.TestCase):
    def trace(self, board, width, size, rate):
        result = subprocess.run([
            'tclsh', str(HERE / 'trace_psd.tcl'),
            f'examples/{board}/fft/tcl/power_spectral_density.tcl',
            str(width), str(size), str(rate),
            *(['use_luts'] if board == 'red-pitaya' else []),
        ], cwd=ROOT, text=True, capture_output=True, check=True)
        return result.stdout

    def test_alpha250_reference_is_preserved(self):
        self.assertEqual(self.trace('alpha250', 16, 8192, 250000000),
                         (HERE / 'alpha250.trace').read_text())

    def test_red_pitaya_uses_reference_with_board_parameters(self):
        expected = (HERE / 'alpha250.trace').read_text()
        for old, new in (
            ('-from 15 -to 0 data', '-from 13 -to 0 data'),
            ('Output_Width 15', 'Output_Width 13'),
            ('transform_length 8192', 'transform_length 2048'),
            ('target_clock_frequency 250.0', 'target_clock_frequency 125.0'),
            ('butterfly_type use_xtremedsp_slices', 'butterfly_type use_luts'),
            ('get_concat_pin_{get_Q_pin_data_3_get_constant_pin_0_16}',
             'get_concat_pin_{get_constant_pin_0_2_get_Q_pin_data_3_get_constant_pin_0_16}'),
        ):
            self.assertIn(old, expected)
            expected = expected.replace(old, new)
        self.assertEqual(self.trace('red-pitaya', 14, 2048, 125000000), expected)

    def test_invalid_width_is_rejected(self):
        with self.assertRaises(subprocess.CalledProcessError):
            self.trace('red-pitaya', 17, 2048, 125000000)


if __name__ == '__main__':
    unittest.main()
