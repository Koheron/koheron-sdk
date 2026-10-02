"""Host-only protocol regression: PYTHONPATH=python python3 this_file.py."""
import importlib.util
from pathlib import Path
import socket
import struct
import unittest

from koheron.koheron import KoheronClient

spec = importlib.util.spec_from_file_location(
    'fft_example', Path(__file__).resolve().parents[1] / 'python/fft.py')
fft_module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fft_module)


class ProtocolTest(unittest.TestCase):
    def test_control_parameters_leave_next_reply_intact(self):
        reader, writer = socket.socketpair()
        self.addCleanup(reader.close)
        self.addCleanup(writer.close)
        reader.settimeout(1)
        client = KoheronClient.__new__(KoheronClient)
        client.sock = reader
        # Supply discovery metadata without starting a hardware server.
        client.get_ids = lambda *_: (12, 9, [])
        client.check_ret_tuple = lambda: None
        client.check_ret_type = lambda _: None
        driver = fft_module.FFT.__new__(fft_module.FFT)
        driver.client = client

        for reference in (0, 2):
            expected = (5e6, 10e6, 250e6, 1, .25, .375, 3, reference)
            writer.sendall(
                struct.pack('>IHHdddIddII', 0, 12, 9, *expected)
                + struct.pack('>IHHI', 0, 12, 6, 8192))
            self.assertEqual(driver.get_control_parameters(), expected)
            self.assertEqual(driver.get_fft_size(), 8192)
            writer.recv(1024)  # Drain the two outgoing requests.


if __name__ == '__main__':
    unittest.main()
