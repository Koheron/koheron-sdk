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
                patch.object(server_init.socket, 'socket') as socket:
            server_init.main()
            return socket.return_value.sendall.call_args_list

    def test_bare_instrument(self):
        calls = self.run_helper([])
        self.assertEqual(len(calls), 1)  # Driver discovery only.

    def test_common_without_led_command(self):
        calls = self.run_helper([{'class': 'Common', 'id': 2, 'functions': []}])
        self.assertEqual(len(calls), 1)

    def test_led_command_is_sent(self):
        calls = self.run_helper([{'class': 'Common', 'id': 2, 'functions': [
            {'name': 'ip_on_leds', 'id': 7}]}])
        self.assertEqual(len(calls), 2)
        self.assertEqual(calls[1].args[0], b'\x00\x00\x00\x00\x00\x02\x00\x07')


if __name__ == '__main__':
    unittest.main()
