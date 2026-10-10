"""Management API behavior, compatibility and RFC6455 lifecycle tests."""
import base64
import concurrent.futures
import hashlib
import json
import os
import socket
import struct
import time
import unittest

from native_fixture import NativeFixture, archive


class WebSocket:
    def __init__(self, port, headers=''):
        self.socket = socket.create_connection(('127.0.0.1', port), timeout=3)
        self.socket.sendall((f'GET /api/events HTTP/1.1\r\nHost: localhost\r\nUpgrade: websocket\r\n'
            'Connection: Upgrade\r\nSec-WebSocket-Version: 13\r\n'
            'Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n' + headers + '\r\n').encode())
        self.buffer = b''
        while b'\r\n\r\n' not in self.buffer:
            self.buffer += self.socket.recv(4096)
        self.headers, self.buffer = self.buffer.split(b'\r\n\r\n', 1)

    def read(self, length):
        while len(self.buffer) < length:
            data = self.socket.recv(65536)
            if not data:
                raise EOFError()
            self.buffer += data
        result, self.buffer = self.buffer[:length], self.buffer[length:]
        return result

    def frame(self):
        first, second = self.read(2)
        length = second & 127
        if length == 126:
            length = struct.unpack('!H', self.read(2))[0]
        elif length == 127:
            length = struct.unpack('!Q', self.read(8))[0]
        assert not second & 128
        return first & 15, self.read(length)

    def send(self, opcode, data=b''):
        mask = b'abcd'
        self.socket.sendall(bytes([128 | opcode, 128 | len(data)]) + mask +
            bytes(value ^ mask[i % 4] for i, value in enumerate(data)))

    def close(self):
        self.socket.close()


class NativeManagementTest(NativeFixture):
    def setUp(self):
        super().setUp()
        (self.root / 'manifest').write_text('board=red-pitaya\n')
        self.proc = self.root / 'proc'; self.proc.mkdir()
        (self.proc / 'uptime').write_text('123.50 0.00\n')
        (self.proc / 'loadavg').write_text('0.10 0.20 0.30 1/2 123\n')
        (self.proc / 'meminfo').write_text('MemTotal: 65536 kB\nMemAvailable: 32768 kB\n')
        self.api_options = ['--proc', str(self.proc)]
        self.start_api()

    def get(self, path):
        status, body, _ = self.request(path)
        self.assertEqual(status, 200, body)
        return json.loads(body)

    def command(self, action):
        status, body, _ = self.request('/api/instruments/' + action, b'', method='POST')
        return status, json.loads(body)

    def package_with_metadata(self, **updates):
        architecture = 'arm64' if os.uname().machine == 'aarch64' else 'amd64'
        metadata = {'format': 1, 'board': 'red-pitaya', 'architecture': architecture,
                    'sdk_version': '1.0', 'min_runtime_api': 1, **updates}
        header = bytearray(64); header[:6] = b'\x7fELF\x02\x01'
        header[18:20] = struct.pack('<H', 183 if architecture == 'arm64' else 62)
        self.package.write_bytes(archive(extra={'serverd': bytes(header), 'instrument.json': json.dumps(metadata).encode()}))

    def test_health_collection_and_loaded_identity(self):
        value = self.get('/api/system/status')
        self.assertEqual(value['health']['uptime_seconds'], 123)
        self.assertEqual(value['health']['memory']['available_bytes'], 32768 * 1024)
        self.assertEqual(value['health']['load_average'], [.1, .2, .3])
        self.assertGreater(value['health']['storage']['staging']['available_bytes'], 0)
        self.assertEqual(value['current_instrument']['name'], 'old')
        self.assertFalse(value['operation']['busy'])

    def test_missing_proc_values_are_unavailable(self):
        (self.proc / 'meminfo').unlink(); (self.proc / 'uptime').unlink()
        health = self.get('/api/system/status')['health']
        self.assertEqual(health['memory'], {})
        self.assertNotIn('uptime_seconds', health)

    def test_preflight_inspects_legacy_without_stopping(self):
        value = self.get('/api/instruments/preflight/new')
        self.assertTrue(value['ready']); self.assertTrue(value['warnings'])
        self.assertGreater(value['required_bytes'], value['archive']['extracted_bytes'])
        self.assertFalse(any(line.startswith('stop ') for line in self.operations()))
        self.assert_old()

    def test_preflight_checks_board_architecture_and_runtime(self):
        for updates, code in (({'board': 'alpha250'}, 'board_mismatch'),
                ({'architecture': 'armhf'}, 'architecture_mismatch'),
                ({'min_runtime_api': 2}, 'runtime_mismatch')):
            with self.subTest(code=code):
                self.package_with_metadata(**updates)
                value = self.get('/api/instruments/preflight/new')
                self.assertFalse(value['ready']); self.assertEqual(value['code'], code)
                status, result = self.command('activate/new')
                self.assertEqual(status, 422); self.assertEqual(result['code'], code)
                self.assertFalse(any(line.startswith('stop ') for line in self.operations()))
                self.assert_old()

    def test_matching_manifest_and_elf_are_ready(self):
        self.package_with_metadata()
        self.assertTrue(self.get('/api/instruments/preflight/new')['ready'])

    def test_elf_architecture_cannot_lie_in_manifest(self):
        self.package_with_metadata()
        data = archive(extra={'serverd': b'\x7fELF\x01\x01' + b'\0' * 12 + b'\x28\x00' + b'\0' * 44})
        self.package.write_bytes(data)
        self.assertEqual(self.get('/api/instruments/preflight/new')['code'], 'architecture_mismatch')

    def test_unsafe_preflight_preserves_running_instrument(self):
        self.package.write_bytes(archive(extra={'../escape': b'bad'}))
        self.assertEqual(self.get('/api/instruments/preflight/new')['code'], 'invalid_archive')
        self.assertEqual(self.command('activate/new')[0], 422)
        self.assert_old()

    def test_stop_start_and_restart_preserve_files(self):
        before = {p.name: p.read_bytes() for p in self.live.iterdir()}
        self.assertEqual(self.command('control/stop')[0], 200)
        value = self.get('/api/system/status')
        self.assertIsNone(value['instruments']['live_instrument'])
        self.assertEqual(value['current_instrument']['name'], 'old')
        self.assertEqual(self.command('control/start')[0], 200)
        self.assertEqual(self.command('control/restart')[0], 200)
        self.assertEqual(before, {p.name: p.read_bytes() for p in self.live.iterdir()})
        self.assertEqual(list(self.root.glob('.instrument-*')), [])

    def test_control_failure_is_specific(self):
        self.save_state(fail_stop=True)
        status, value = self.command('control/stop')
        self.assertEqual(status, 500); self.assertEqual(value['code'], 'stop_failed')
        self.assertFalse(self.get('/api/system/status')['operation']['busy'])

    def test_commands_require_post_and_no_body(self):
        self.assertEqual(self.request('/api/instruments/control/stop')[0], 405)
        self.assertEqual(self.request('/api/instruments/control/stop', b'body', method='POST')[0], 400)
        self.assertFalse(any(line.startswith('stop ') for line in self.operations()))

    def test_default_selection_is_persistent_and_does_not_activate(self):
        self.assertEqual(self.command('default/new')[0], 200)
        self.assertEqual((self.store / 'default').read_text(), 'new.zip\n')
        self.assertFalse(any(line.startswith('stop ') for line in self.operations()))
        self.assertEqual(list(self.store.glob('.preference-*')), [])
        self.stop_api(); self.start_api()
        values = self.get('/api/instruments/details')['instruments']
        self.assertEqual([i['name'] for i in values if i['is_default']], ['new'])
        self.assertIn(b'Default instrument cannot be removed', self.request('/api/instruments/delete/new')[1])

    def test_invalid_default_preserves_preference(self):
        self.package.write_bytes(b'bad')
        self.assertEqual(self.command('default/new')[0], 422)
        self.assertEqual((self.store / 'default').read_text(), 'old.zip\n')

    def test_failed_activation_reports_rollback_outcome(self):
        self.save_state(fail_new=True)
        status, value = self.command('activate/new')
        self.assertEqual(status, 500); self.assertEqual(value['rollback'], 'restored')
        self.assertEqual(value['code'], 'start_failed'); self.assert_old()
        self.save_state(fail_new=True, fail_old=True)
        self.assertEqual(self.command('activate/new')[1]['rollback'], 'failed')

    def test_diagnostics_download_contains_bounded_context(self):
        status, body, headers = self.request('/api/system/diagnostics')
        self.assertEqual(status, 200); value = json.loads(body)
        self.assertEqual(value['manifest']['board'], 'red-pitaya')
        self.assertIn('health', value); self.assertIn('logs', value)
        self.assertLess(len(body), 1024 * 1024)
        self.assertIn('attachment', headers['Content-Disposition'])

    def test_websocket_handshake_snapshot_and_ping(self):
        stream = WebSocket(self.port); self.addCleanup(stream.close)
        self.assertIn(b'101 Switching Protocols', stream.headers)
        self.assertIn(b's3pPLMBiTxaQ9kYGzzhZRbK+xOo=', stream.headers)
        opcode, data = stream.frame(); self.assertEqual(opcode, 1)
        self.assertEqual(json.loads(data)['type'], 'status')
        stream.send(9, b'hello')
        opcode, payload = stream.frame(); self.assertEqual((opcode, payload), (10, b'hello'))
        stream.send(8, struct.pack('!H', 1000))
        self.assertEqual(stream.frame()[0], 8)

    def test_websocket_rejects_foreign_origin_and_invalid_handshake(self):
        stream = WebSocket(self.port, 'Origin: http://foreign\r\n'); self.addCleanup(stream.close)
        self.assertIn(b'403', stream.headers)
        self.assertEqual(self.request('/api/events')[0], 426)

    def test_websocket_rejects_unmasked_frames_and_invalid_close_codes(self):
        for payload in (b'\x89\x01x', b'\x88\x82abcd' + bytes([0x03 ^ ord('a'), 0xed ^ ord('b')])):
            stream = WebSocket(self.port)
            try:
                stream.frame(); stream.socket.sendall(payload)
                opcode, value = stream.frame()
                self.assertEqual(opcode, 8); self.assertEqual(struct.unpack('!H', value[:2])[0], 1002)
            finally: stream.close()

    def test_websocket_connection_budget_leaves_http_available(self):
        streams = []
        try:
            for _ in range(8):
                stream = WebSocket(self.port); streams.append(stream); stream.frame()
            extra = WebSocket(self.port); streams.append(extra)
            self.assertIn(b'503', extra.headers)
            self.assertEqual(self.get('/api/system/status')['type'], 'status')
        finally:
            for stream in streams: stream.close()

    def test_websocket_stays_responsive_during_activation(self):
        stream = WebSocket(self.port); self.addCleanup(stream.close)
        self.assertFalse(json.loads(stream.frame()[1])['operation']['busy'])
        gate = self.root / 'hold-start'; gate.touch()
        with concurrent.futures.ThreadPoolExecutor(max_workers=1) as pool:
            activation = pool.submit(self.command, 'activate/new')
            try:
                deadline = time.monotonic() + 3
                while not (self.root / 'start-entered').exists():
                    self.assertLess(time.monotonic(), deadline); time.sleep(.01)
                while True:
                    value = json.loads(stream.frame()[1])
                    if value['operation']['phase'] == 'starting': break
                    self.assertLess(time.monotonic(), deadline)
                self.assertTrue(value['operation']['busy'])
                self.assertEqual(self.command('control/stop')[0], 409)
            finally:
                gate.unlink()
            self.assertEqual(activation.result(timeout=3)[0], 200)
        while True:
            value = json.loads(stream.frame()[1])
            if value['operation']['phase'] == 'succeeded': break
            self.assertLess(time.monotonic(), deadline)
        self.assertEqual(value['operation']['phase'], 'succeeded')
        self.assertEqual(value['instruments']['live_instrument']['name'], 'new')
        self.stop_api()  # An open upgraded connection must not prevent shutdown.


if __name__ == '__main__':
    unittest.main()
