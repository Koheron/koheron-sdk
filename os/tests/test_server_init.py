"""The optional LED helper must also work with bare instruments."""
import importlib.util
from pathlib import Path
import unittest
from unittest.mock import patch


SCRIPT = Path(__file__).resolve().parents[1] / 'scripts/koheron-server-init.py'
spec = importlib.util.spec_from_file_location('server_init', SCRIPT)
server_init = importlib.util.module_from_spec(spec)
spec.loader.exec_module(server_init)


class ServerInitTest(unittest.TestCase):
    def run_helper(self, drivers):
        with patch.object(server_init.KoheronClient, 'recv_json', return_value=drivers), \
                patch.object(server_init, 'wait_for_ipv4') as wait, \
                patch.object(server_init.socket, 'socket') as socket:
            server_init.main()
            self.wait_calls = wait.call_count
            return socket.return_value.sendall.call_args_list

    def test_bare_instrument(self):
        calls = self.run_helper([])
        self.assertEqual(len(calls), 1)  # Driver discovery only.
        self.assertEqual(self.wait_calls, 0)

    def test_common_without_led_command(self):
        calls = self.run_helper([{'class': 'Common', 'id': 2, 'functions': []}])
        self.assertEqual(len(calls), 1)
        self.assertEqual(self.wait_calls, 0)

    def test_led_command_is_sent(self):
        calls = self.run_helper([{'class': 'Common', 'id': 2, 'functions': [
            {'name': 'ip_on_leds', 'id': 7}]}])
        self.assertEqual(len(calls), 2)
        self.assertEqual(calls[1].args[0], b'\x00\x00\x00\x00\x00\x02\x00\x07')
        self.assertEqual(self.wait_calls, 1)

    def test_late_dhcp_is_retried(self):
        with patch.object(server_init, 'has_ipv4_address',
                          side_effect=[False, False, False, True]), \
                patch.object(server_init.time, 'sleep') as sleep:
            server_init.wait_for_ipv4()
        self.assertEqual(sleep.call_args_list, [unittest.mock.call(1)] * 2)

    def test_missing_interfaces_are_not_ready(self):
        with patch.object(server_init.socket, 'socket'), \
                patch.object(server_init.fcntl, 'ioctl', side_effect=OSError):
            self.assertFalse(server_init.has_ipv4_address())

    def test_legacy_interface_is_accepted(self):
        with patch.object(server_init.socket, 'socket'), \
                patch.object(server_init.fcntl, 'ioctl',
                             side_effect=[OSError(), b'\x00' * 20 + b'\xc0\xa8\x01\x0f']) as ioctl:
            self.assertTrue(server_init.has_ipv4_address())
        self.assertEqual(ioctl.call_args.args[2].rstrip(b'\x00'), b'eth0')

    def test_unspecified_address_is_not_ready(self):
        with patch.object(server_init.socket, 'socket'), \
                patch.object(server_init.fcntl, 'ioctl', return_value=b'\x00' * 24):
            self.assertFalse(server_init.has_ipv4_address())


if __name__ == '__main__':
    unittest.main()
