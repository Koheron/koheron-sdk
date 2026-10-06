"""Check the DPLL client's common PNA reply shapes without board access."""
import importlib.util
import struct
import sys
import unittest
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / 'python'))
spec = importlib.util.spec_from_file_location('dpll_monitor_client', ROOT / 'examples/alpha250/dpll/test_time.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class FakeClient:
    def __init__(self):
        self.calls = []
        self.phase = np.linspace(0, .1, 65536, dtype='<f4')
        self.payload = b''

    def get_ids(self, device, name):
        self.calls.append((device, name))
        return 1, name, ()

    def send_command(self, *_):
        pass

    def recv_uint32(self):
        return 65536 if self.calls[-1][1] == 'get_data_size' else 6250000

    def recv_array(self, size, dtype):
        assert size == 65536 and dtype == 'float32'
        return self.phase.copy()

    def recv_tuple(self, fmt):
        if fmt == 'QIf?':
            self.payload = self.phase.tobytes()
            return 12, 8, .000006, True
        assert fmt == 'QIIdIIIIIddddIdI'
        self.payload = struct.pack('>I', 12) + np.array([0, 2e-12, 4e-12], dtype='<f4').tobytes()
        return 13, 1, 8, 6250000., 0, 20, 4, 4, 4, 10e6, 12e6, 0., 0., 0, 0., 2

    def recv_all(self, size):
        data, self.payload = self.payload[:size], self.payload[size:]
        assert len(data) == size
        return data


class MonitorClientTests(unittest.TestCase):
    def setUp(self):
        self.client = FakeClient()
        self.driver = module.Dma(self.client)

    def test_phase_reply_uses_65536_calibrated_float_samples(self):
        self.assertEqual(self.driver.get_data_size(), 65536)
        phase = self.driver.get_data()
        self.assertEqual(phase.dtype, np.dtype('float32'))
        np.testing.assert_array_equal(phase, self.client.phase)
        self.assertEqual(self.driver.get_sampling_frequency(), 6250000)
        self.assertEqual(self.client.calls, [('Dma', 'get_data_size'), ('Dma', 'get_data'), ('Dma', 'get_sampling_frequency')])

    def test_phase_snapshot_keeps_sequence_validity_and_calibration_in_one_reply(self):
        sequence, bits, scale, valid, phase = self.driver.get_phase_snapshot()
        self.assertEqual((sequence, bits, scale, valid), (12, 8, .000006, True))
        np.testing.assert_array_equal(phase, self.client.phase)
        self.assertEqual(self.client.payload, b'')
        self.assertEqual(self.client.calls, [('Dma', 'get_phase_snapshot')])

    def test_shared_spectrum_decoder_targets_dpll_monitor(self):
        metadata, density = self.driver.get_spectrum_snapshot()
        self.assertEqual(metadata[:4], (13, 1, 8, 6250000.))
        np.testing.assert_array_equal(density, np.array([0, 2e-12, 4e-12], dtype='<f4'))
        self.assertEqual(self.client.payload, b'')
        self.assertEqual(self.client.calls, [('Dma', 'get_spectrum_snapshot')])


if __name__ == '__main__':
    unittest.main()
