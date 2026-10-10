"""Browser boundary, metadata and idle-stream regression tests."""
import json
import os
import struct
import time
import unittest

from native_fixture import NativeFixture, archive
from test_native_management import WebSocket


class NativeHardeningTest(NativeFixture):
    def setUp(self):
        super().setUp()
        self.start_api()

    def test_foreign_browser_requests_cannot_mutate_or_stage_uploads(self):
        for headers in ({'Origin': 'http://foreign'}, {'Origin': 'null'},
                {'Referer': 'http://foreign/page'}, {'Sec-Fetch-Site': 'cross-site'},
                {'Sec-Fetch-Site': 'same-site'}):
            for path, method in (('run/new', 'GET'), ('delete/new', 'GET'),
                    ('activate/new', 'POST'), ('default/new', 'POST'),
                    ('control/stop', 'POST'), ('upload', 'POST')):
                with self.subTest(headers=headers, path=path):
                    status, body, _ = self.request('/api/instruments/' + path,
                        b'ignored' if method == 'POST' else None, headers, method)
                    self.assertEqual(status, 403, body)
                    self.assertEqual(json.loads(body)['code'], 'origin_rejected')
        self.assertEqual(list(self.store.glob('.upload-*')), [])
        self.assertEqual(self.package.read_bytes(), archive())
        self.assertFalse(any(line.startswith('stop ') for line in self.operations()))
        self.assert_old()

    def test_same_origin_with_port_and_cli_commands_remain_supported(self):
        origin = f'http://127.0.0.1:{self.port}'
        status, body, _ = self.request('/api/instruments/control/stop', b'',
            {'Origin': origin, 'Referer': origin + '/koheron/', 'Sec-Fetch-Site': 'same-origin'}, 'POST')
        self.assertEqual(status, 200, body)
        self.assertEqual(self.request('/api/instruments/run/new')[0], 200)

    def test_browser_navigation_cannot_trigger_legacy_commands(self):
        for headers in ({'Sec-Fetch-Mode': 'navigate'}, {'Sec-Fetch-Dest': 'image'},
                {'Sec-Fetch-Dest': 'document'}):
            status, _, _ = self.request('/api/instruments/run/new', headers=headers)
            self.assertEqual(status, 403)
        self.assert_old()

    def test_head_requests_never_activate_or_delete(self):
        for path in ('run/new', 'delete/new'):
            self.assertEqual(self.request('/api/instruments/' + path, method='HEAD')[0], 405)
        self.assertTrue(self.package.exists())
        self.assertFalse(any(line.startswith('stop ') for line in self.operations()))
        self.assert_old()

    def test_reflected_legacy_errors_are_plain_text(self):
        _, body, headers = self.request('/api/instruments/run/%3Csvg%20onload%3Dalert(1)%3E')
        self.assertIn(b'<svg', body)
        self.assertEqual(headers.get_content_type(), 'text/plain')
        self.assertEqual(headers['X-Content-Type-Options'], 'nosniff')

    def test_compatibility_metadata_rejects_invalid_json(self):
        architecture = 'arm64' if os.uname().machine == 'aarch64' else 'amd64'
        metadata = json.dumps({'format': 1, 'board': 'red-pitaya', 'architecture': architecture,
            'sdk_version': '1.0', 'min_runtime_api': 1}, separators=(',', ':')).encode()
        header = bytearray(64); header[:6] = b'\x7fELF\x02\x01'
        header[18:20] = struct.pack('<H', 183 if architecture == 'arm64' else 62)
        self.package.write_bytes(archive(extra={'serverd': bytes(header), 'instrument.json': metadata}))
        self.assertTrue(json.loads(self.request('/api/instruments/preflight/new')[1])['ready'])
        for value in (metadata + b' ignored', metadata + b' {}',
                metadata.replace(b'red-pitaya', b'red-\xffpitaya'), metadata.replace(b':1,', b':01,')):
            with self.subTest(value=value):
                self.package.write_bytes(archive(extra={'serverd': bytes(header), 'instrument.json': value}))
                status, body, _ = self.request('/api/instruments/preflight/new')
                self.assertEqual(status, 200)
                checked = json.loads(body)
                self.assertFalse(checked['ready'])
                self.assertEqual(checked['code'], 'invalid_archive')
        self.assert_old()

    def test_unresponsive_stream_expires_while_pong_peer_survives(self):
        idle = WebSocket(self.port); healthy = WebSocket(self.port)
        self.addCleanup(idle.close); self.addCleanup(healthy.close)
        idle.socket.settimeout(3); healthy.socket.settimeout(3)
        started = time.monotonic(); ping_seen = False
        while time.monotonic() - started < 17:
            opcode, payload = healthy.frame()
            if opcode == 9:
                ping_seen = True
                healthy.send(10, payload)
                # A mismatched pong must not keep an idle slot occupied.
                idle.send(10, b'wrong')
            self.assertNotEqual(opcode, 8)
            if ping_seen and time.monotonic() - started >= 16:
                break
        self.assertTrue(ping_seen)
        idle.socket.settimeout(1)
        with self.assertRaises(EOFError):
            while True:
                idle.frame()
        self.assertEqual(self.request('/api/system/status')[0], 200)

    def test_replacing_boot_default_rejects_invalid_packages_without_committing(self):
        architecture = 'arm64' if os.uname().machine == 'aarch64' else 'amd64'
        header = bytearray(64); header[:6] = b'\x7fELF\x02\x01'
        header[18:20] = struct.pack('<H', 183 if architecture == 'arm64' else 62)
        incompatible = archive(extra={'serverd': bytes(header), 'instrument.json': json.dumps({
            'format': 1, 'board': 'red-pitaya', 'architecture': 'armhf', 'sdk_version': '1.0', 'min_runtime_api': 1}).encode()})
        for data in (archive(executable=False), archive(extra={'../escape': b'unsafe'}), incompatible):
            with self.subTest(data=data):
                status, body, _ = self.upload(data, 'old.zip')
                self.assertEqual(status, 422, body)
                self.assertEqual((self.store / 'old.zip').read_bytes(), self.original)
                self.assertEqual((self.store / 'default').read_text(), 'old.zip\n')
                self.assertEqual(list(self.store.glob('.upload-*')), [])
        self.assert_old()

    def test_valid_boot_default_replacement_keeps_selection_and_loaded_identity(self):
        data = archive(version='updated')
        self.assertEqual(self.upload(data, 'old.zip')[0], 200)
        self.assertEqual((self.store / 'old.zip').read_bytes(), data)
        self.stop_api(); self.start_api()
        value = json.loads(self.request('/api/instruments/details')[1])
        self.assertEqual(next(item for item in value['instruments'] if item['is_default'])['version'], 'updated')
        self.assertEqual(value['live_instrument']['version'], 'loaded-version')
        self.assert_old()


if __name__ == '__main__':
    unittest.main()
