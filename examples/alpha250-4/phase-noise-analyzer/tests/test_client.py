import sys
import unittest
from pathlib import Path
from unittest.mock import patch

import numpy as np

ROOT = Path(__file__).resolve().parents[4]
sys.path[:0] = [str(ROOT / 'python'), str(Path(__file__).resolve().parents[1] / 'python')]
from phase_noise_analyzer import PhaseNoiseAnalyzer, smooth_phase_psd_logfreq


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

    def recv_bool(self):
        return True

    def recv_all(self, size):
        return np.arange(size // 4, dtype='<f4').tobytes()

    def recv_tuple(self, fmt):
        if fmt == 'IIdIQQQdd':
            return (8, 8, .000006, 1, 100, 0, 0, 10., 90.)
        if fmt == 'QII?':
            return (3, 120, 8192, False)
        if fmt == 'QI?':
            return (100, 8, True)
        if fmt == 'QIf?':
            return (100, 8, .000006, True)
        if fmt == 'dddd':
            return (10e6, 10e6 + .001, 10e6, 10e6)
        if fmt == 'II':
            return (42, 0)
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

    def test_precision_and_atomic_snapshot(self):
        self.assertTrue(self.driver.set_phase_precision(8))
        self.assertEqual(self.driver.get_precision_status()[:2], (8, 8))
        snapshot = self.driver.get_phase_snapshot()
        self.assertEqual(snapshot[:2], (100, 8))
        self.assertTrue(snapshot[2])
        np.testing.assert_array_equal(snapshot[3], np.arange(65536, dtype=np.float32))
        self.assertEqual(snapshot[4][0], 65536)

    def test_acquisition_gap_and_fifo_status(self):
        self.assertEqual(self.driver.get_acquisition_status(), (3, 120, 8192, False))
        self.assertEqual(self.client.commands, [('get_acquisition_status', ())])

    def test_dds_command_preserves_precision(self):
        frequency = 10e6 + 0.637
        self.driver.set_local_oscillator(1, frequency)
        self.assertEqual(self.client.commands, [('set_local_oscillator', (1, frequency))])

    def test_nominal_frequencies_and_cumulative_status(self):
        self.assertEqual(self.driver.get_nominal_frequencies()[1], 10e6 + .001)
        self.assertEqual(self.driver.get_average_status(), (42, 0))
        self.assertEqual(self.client.commands, [('get_nominal_frequencies', ()),
                                                ('get_average_status', ())])

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

    def test_signed_smoothing_keeps_cancellation(self):
        freqs = np.array([0., 99., 100., 101.])
        psd = np.array([0., 3., -2., 1.])
        result = smooth_phase_psd_logfreq(freqs, psd, nstart=1)
        expected = 10 * np.log10((3 - 2 + 1) / 3 / 2)
        self.assertAlmostEqual(result[2], expected)
        # A negative mean cannot be displayed as positive phase-noise power.
        result = smooth_phase_psd_logfreq(freqs, np.array([0., 1., -4., 1.]), nstart=1)
        self.assertTrue(np.isnan(result[2]))

    def test_smoothing_respects_first_valid_bin(self):
        result = smooth_phase_psd_logfreq(np.array([0., 99., 100., 101.]),
                                         np.array([0., 1000., 2., 2.]), nstart=2)
        self.assertTrue(np.isnan(result[1]))
        self.assertAlmostEqual(result[2], 0.)

    def test_frequency_noise_uses_linear_phase_psd(self):
        self.client.recv_vector = lambda dtype: np.array([2., 2., -1., 0.], dtype=dtype)
        freqs, density = self.driver.frequency_noise()
        self.assertAlmostEqual(density[1], 10 * np.log10(2 * freqs[1] ** 2))
        self.assertTrue(np.isnan(density[[0, 2, 3]]).all())

    @patch('phase_noise_analyzer.time.sleep')
    def test_acquisition_preserves_signed_spectrum(self, sleep):
        values = np.ones(15001, dtype='float32')
        values[100] = -2
        self.client.recv_vector = lambda dtype: values
        self.driver.phase_noise(min_count=1, verbose=False)
        np.testing.assert_array_equal(self.driver.last_phase_psd, values)


if __name__ == '__main__':
    unittest.main()
