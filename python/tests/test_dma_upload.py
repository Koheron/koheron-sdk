"""The DMA example clients upload bounded chunks and wait for acknowledgments."""
import importlib.util
from pathlib import Path
import struct
import sys
import unittest
from unittest.mock import patch

import numpy as np
from koheron.koheron import make_command

EXAMPLE = Path(__file__).resolve().parents[2] / 'examples/alpha250/adc-dac-dma'


def load_module(name, filename):
    spec = importlib.util.spec_from_file_location(name, EXAMPLE / filename)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


upload = load_module('dac_upload', 'dac_upload.py')
with patch.dict(sys.modules, {'dac_upload': upload}):
    CLIENTS = tuple(load_module(name, name + '.py').AdcDacDma
                   for name in ('test', 'bode_analyzer'))


class RecordingClient:
    def __init__(self, reject_at=None, disconnect_at=None):
        self.packets = []
        self.pending = False
        self.reject_at = reject_at
        self.disconnect_at = disconnect_at

    def get_ids(self, classname, method):
        assert (classname, method) == ('AdcDacDma', 'set_dac_data_chunk')
        return 2, 17, [{'name': 'offset', 'type': 'uint32_t'},
                       {'name': 'dac_data', 'type': 'const std::vector<uint32_t>&'}]

    def send_command(self, driver, operation, signature, *args):
        assert not self.pending, 'Sent next chunk before reading acknowledgment'
        self.packets.append(make_command(driver, operation, signature, *args))
        self.pending = True

    def recv_bool(self):
        assert self.pending
        self.pending = False
        index = len(self.packets) - 1
        if index == self.disconnect_at:
            raise ConnectionError('connection closed')
        return index != self.reject_at


class DmaUploadClientTest(unittest.TestCase):
    def check_upload(self, data):
        for client_class in CLIENTS:
            with self.subTest(client=client_class.__module__, words=data.size):
                client = RecordingClient()
                client_class(client).set_dac_data(data)
                offset = 0
                received = bytearray()
                for packet in client.packets:
                    reserved, driver, operation, actual_offset, length = struct.unpack('>IHHII', packet[:16])
                    self.assertEqual((reserved, driver, operation), (0, 2, 17))
                    self.assertEqual(actual_offset, offset)
                    self.assertLessEqual(length, 128 * 1024)
                    self.assertEqual(len(packet), 16 + length)
                    received.extend(packet[16:])
                    offset += length // 4
                self.assertEqual(received, data.tobytes())
                self.assertFalse(client.pending)

    def test_two_descriptors_and_chunk_boundaries(self):
        for size in (0, 1, 32768, 32769, 65536, 131072, 131073):
            self.check_upload(np.arange(size, dtype=np.uint32) * 65537)

    def test_noncontiguous_waveform(self):
        self.check_upload(np.arange(131072, dtype=np.uint32)[::2])

    def test_rejected_chunk_stops_upload(self):
        for client_class in CLIENTS:
            client = RecordingClient(reject_at=1)
            with self.assertRaisesRegex(RuntimeError, 'word offset 32768'):
                client_class(client).set_dac_data(np.zeros(131072, dtype=np.uint32))
            self.assertEqual(len(client.packets), 2)

    def test_disconnect_stops_upload(self):
        for client_class in CLIENTS:
            client = RecordingClient(disconnect_at=1)
            with self.assertRaises(ConnectionError):
                client_class(client).set_dac_data(np.zeros(131072, dtype=np.uint32))
            self.assertEqual(len(client.packets), 2)

    def test_invalid_waveform_is_rejected_before_any_write(self):
        oversized = np.broadcast_to(np.uint32(0), (upload.N_PTS * upload.N_DESC_MAX + 1,))
        for data, error in ((oversized, ValueError),
                            (np.zeros((2, 2), dtype=np.uint32), ValueError),
                            (np.zeros(4, dtype=np.uint16), TypeError),
                            (np.zeros(4), TypeError)):
            for client_class in CLIENTS:
                client = RecordingClient()
                with self.assertRaises(error):
                    client_class(client).set_dac_data(data)
                self.assertEqual(client.packets, [])


if __name__ == '__main__':
    unittest.main()
