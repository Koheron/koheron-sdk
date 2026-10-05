"""Exercise the actual Python RPC client and its spectral normalization."""
import sys
import unittest
from pathlib import Path

import numpy as np
from scipy import signal

ROOT = Path(__file__).resolve().parents[4]
sys.path[:0] = [str(ROOT / "python"), str(Path(__file__).resolve().parents[1])]
from phase_noise_analyzer import PhaseNoiseAnalyzer


class FakeClient:
    def __init__(self):
        self.commands = []
        self.sample_rate = 3_125_000.
        t = np.arange(65536)
        self.phase = (0.001 * t + 0.1 * np.sin(2 * np.pi * 64 * t / 65536)).astype("float32")

    def get_ids(self, device, name):
        return 1, name, ()

    def send_command(self, device, name, types, *args):
        self.commands.append((device, name, args))

    def recv_bool(self):
        return True

    def recv_all(self, size):
        return np.arange(size // 4, dtype='<f4').tobytes()

    def recv_tuple(self, fmt):
        if fmt == 'IIdIQQQdd':
            return (8, 8, .000006, 1, 100, 0, 0, 10., 90.)
        if fmt == 'QI?':
            return (100, 8, True)
        if fmt == 'QIf?':
            return (100, 8, .000006, True)
        assert fmt == "IfIIIddIfI"
        return 16385, self.sample_rate, 0, 32, 1, 10e6 + .637, 10e6, 0, 0., 0

    def recv_array(self, size, dtype):
        assert size == self.phase.size and dtype == "float32"
        return self.phase.copy()

    def recv_vector(self, dtype):
        return np.ones(16385, dtype=dtype)

    def recv_double(self):
        return -3.


class ClientTests(unittest.TestCase):
    def setUp(self):
        self.client = FakeClient()
        self.driver = PhaseNoiseAnalyzer(self.client)

    def test_precision_and_atomic_snapshot(self):
        self.assertTrue(self.driver.set_phase_precision(8))
        self.assertEqual(self.driver.get_precision_status()[:2], (8, 8))
        snapshot = self.driver.get_phase_snapshot()
        self.assertEqual(snapshot[:2], (100, 8))
        self.assertTrue(snapshot[3])
        np.testing.assert_array_equal(snapshot[4], np.arange(65536, dtype=np.float32))

    def test_compatibility_dds_setter_uses_analyzer_invalidation(self):
        self.driver.set_dds_freq(1, 10e6 + .637)
        self.assertEqual(self.client.commands, [(1, "set_local_oscillator", (1, 10e6 + .637))])

    def test_snapshot_sizes_and_fractional_reference(self):
        self.assertEqual(self.driver.get_phase().shape, (65536,))
        self.assertEqual(self.driver.get_phase_noise().shape, (16385,))
        self.assertEqual(self.driver.get_parameters()[5], 10e6 + .637)

    def test_density_matches_independent_scipy_periodogram(self):
        # No set_cic_rate call: use the actual server-reported rate.
        freq, density_db = self.driver.phase_noise(navg=2)
        expected_freq, expected = signal.periodogram(
            self.client.phase, fs=self.client.sample_rate,
            window="hann", detrend="linear", scaling="density")
        np.testing.assert_array_equal(freq, expected_freq)
        self.assertEqual(freq[-1], self.client.sample_rate / 2)
        # SciPy's single precision FFT and NumPy's FFT differ in the tiny floor.
        np.testing.assert_allclose(2 * 10 ** (density_db[63:66] / 10), expected[63:66], rtol=1e-4)
        df = freq[1]
        self.assertAlmostEqual(np.sum(2 * 10 ** (density_db[62:67] / 10)) * df, .1 ** 2 / 2, places=5)
        self.assertLess(density_db[1], density_db[64] - 30)
        self.assertEqual(len([c for c in self.client.commands if c[1] == "get_phase"]), 2)

    def test_dc_and_nyquist_are_not_doubled(self):
        n = np.arange(65536)
        self.client.phase = (-1.0) ** n
        freq, density_db = self.driver.phase_noise(window="boxcar")
        _, expected = signal.periodogram(self.client.phase, fs=self.client.sample_rate,
                                        window="boxcar", detrend="linear", scaling="density")
        self.assertAlmostEqual(2 * 10 ** (density_db[-1] / 10), expected[-1], places=12)

    def test_invalid_average_count_sends_no_commands(self):
        for count in (0, -1, 1.5, np.nan):
            with self.assertRaises(ValueError):
                self.driver.phase_noise(count)
        self.assertEqual(self.client.commands, [])

    def test_frequency_noise_uses_exact_phase_density_conversion(self):
        freq, phase_db = self.driver.phase_noise()
        freq2, frequency_db = self.driver.frequency_noise()
        np.testing.assert_array_equal(freq, freq2)
        np.testing.assert_allclose(frequency_db[1:], phase_db[1:] + 10 * np.log10(2) + 20 * np.log10(freq[1:]))


if __name__ == "__main__":
    unittest.main()
