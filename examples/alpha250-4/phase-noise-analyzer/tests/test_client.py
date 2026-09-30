import sys
import unittest
from pathlib import Path
from unittest.mock import patch

import numpy as np

ROOT = Path(__file__).resolve().parents[4]
sys.path[:0] = [str(ROOT / 'python'), str(Path(__file__).resolve().parents[1] / 'python')]
from phase_noise_analyzer import PhaseNoiseAnalyzer


class FakeClient:
    def __init__(self):
        self.commands = []
        self.count = 0

    def get_ids(self, device, name):
        return 1, name, ()

    def send_command(self, device, name, types, *args):
        self.commands.append((name, args))
        if name == 'reset_cumulative_averager':
            self.count = 0

    def recv_tuple(self, fmt):
        self.count += 1
        return (15001, 5e6, 2, 5e6 / 15000, 1, 10e6, 10e6, 10e6, 10e6, 0, self.count)

    def recv_array(self, size, dtype):
        return np.arange(size, dtype=dtype)

    def recv_vector(self, dtype):
        return np.ones(15001, dtype=dtype)


class ClientTests(unittest.TestCase):
    def setUp(self):
        self.client = FakeClient()
        self.driver = PhaseNoiseAnalyzer(self.client)

    def test_dds_command_preserves_precision(self):
        frequency = 10e6 + 0.637
        self.driver.set_local_oscillator(1, frequency)
        self.assertEqual(self.client.commands, [('set_local_oscillator', (1, frequency))])

    def test_axis_matches_received_spectrum(self):
        freqs, low, high = self.driver.get_freqs(15001)
        self.assertEqual(len(freqs), 15001)
        self.assertAlmostEqual(freqs[-1], 2.5e6)
        self.assertAlmostEqual(low, 2 * 5e6 / 30000)
        self.assertAlmostEqual(high, 1.875e6)

    def test_synchronous_channels_have_equal_length(self):
        x, y = self.driver.get_phase_xy_sync()
        self.assertEqual(len(x), 65536)
        self.assertEqual(len(y), 65536)
        self.assertEqual(y[-1], 131071)

    @patch('phase_noise_analyzer.time.sleep')
    def test_cross_spectrum_acquisition(self, sleep):
        f, low, high, raw, smooth = self.driver.phase_noise(min_count=3, remove_spurs=False, verbose=False)
        self.assertEqual(self.client.commands[0], ('set_channel', (2,)))
        self.assertGreaterEqual(self.client.count, 3)
        self.assertAlmostEqual(f[-1], 2.5e6)
        self.assertAlmostEqual(raw[100], 10 * np.log10(0.5), places=6)
        self.assertAlmostEqual(smooth[100], raw[100])


if __name__ == '__main__':
    unittest.main()
