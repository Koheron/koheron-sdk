"""Exercise the compiled LED helper against a real Unix RPC peer."""
import concurrent.futures
import json
import socket
import struct
import subprocess

from native_fixture import BIN_DIR, NativeFixture


class NativeLedTest(NativeFixture):
    def exchange(self, metadata, *, skip=True, declared=None, truncate=False):
        path = self.root / 'rpc.sock'
        with socket.socket(socket.AF_UNIX) as listener:
            listener.bind(str(path)); listener.listen(); listener.settimeout(3)
            def peer():
                with listener.accept()[0] as connection:
                    connection.settimeout(3)
                    request = connection.recv(8, socket.MSG_WAITALL)
                    self.assertEqual(request, struct.pack('!IHH', 0, 1, 1))
                    payload = json.dumps(metadata).encode()
                    header = struct.pack('!III', 0, 0, len(payload) if declared is None else declared)
                    if truncate:
                        connection.sendall(header[:5]); return b''
                    # Fragment the reply to exercise receive loops.
                    for byte in header + payload:
                        connection.sendall(bytes([byte]))
                    return connection.recv(8, socket.MSG_WAITALL)
            with concurrent.futures.ThreadPoolExecutor(max_workers=1) as pool:
                future = pool.submit(peer)
                result = subprocess.run([str(BIN_DIR / 'koheron-server-init'), str(path),
                    *(['--skip-ip-wait'] if skip else [])], capture_output=True, timeout=4)
                packet = future.result(timeout=4)
        return result, packet

    def test_fragmented_metadata_uses_discovered_ids(self):
        result, packet = self.exchange([{'class': 'Common', 'id': 51,
            'functions': [{'name': 'ip_on_leds', 'id': 309}]}])
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(packet, struct.pack('!IHH', 0, 51, 309))

    def test_driver_without_leds_exits_before_network_wait(self):
        for metadata in ([], [{'class': 'Common', 'functions': []}], [{'class': 'Other'}]):
            with self.subTest(metadata=metadata):
                result, packet = self.exchange(metadata, skip=False)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(packet, b'')
                (self.root / 'rpc.sock').unlink()

    def test_bad_identifier_is_rejected(self):
        result, packet = self.exchange([{'class': 'Common', 'id': -1,
            'functions': [{'name': 'ip_on_leds', 'id': 1}]}])
        self.assertEqual(result.returncode, 1); self.assertEqual(packet, b'')

    def test_truncated_metadata_fails(self):
        result, _ = self.exchange([], truncate=True)
        self.assertEqual(result.returncode, 1)

    def test_non_array_metadata_fails(self):
        result, _ = self.exchange({})
        self.assertEqual(result.returncode, 1)
