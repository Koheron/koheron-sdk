import concurrent.futures
import json
import socket
import time
import unittest
import zipfile

from native_fixture import NativeFixture, archive


class NativeApiTest(NativeFixture):
    def test_pending_activation_does_not_admit_more_status_readers(self):
        gate = self.root / 'hold-status'
        gate.touch()
        with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
            snapshot = pool.submit(self.details)
            try:
                deadline = time.monotonic() + 3
                while not (self.root / 'status-entered').exists():
                    self.assertLess(time.monotonic(), deadline); time.sleep(.01)
                activation = pool.submit(self.request, '/api/instruments/run/new')
                time.sleep(.05)
                later = pool.submit(self.request, '/api/instruments/details')
                status, body, _ = later.result(timeout=1)
                self.assertEqual(status, 200)
                self.assertIsNone(json.loads(body)['live_instrument'])
                self.assertFalse(any(line.startswith('stop ') for line in self.operations()))
            finally:
                gate.unlink()
            self.assertEqual(snapshot.result(timeout=5)['live_instrument']['name'], 'old')
            self.assertEqual(activation.result(timeout=5)[0], 200)
        self.assertEqual(self.details()['live_instrument']['name'], 'new')

    def test_status_snapshot_cannot_mix_two_installations(self):
        gate = self.root / 'hold-status'
        gate.touch()
        with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
            snapshot = pool.submit(self.details)
            try:
                deadline = time.monotonic() + 3
                while not (self.root / 'status-entered').exists():
                    self.assertLess(time.monotonic(), deadline); time.sleep(.01)
                activation = pool.submit(self.request, '/api/instruments/run/new')
                time.sleep(.05)
                self.assertFalse(any(line.startswith('stop ') for line in self.operations()))
            finally:
                gate.unlink()
            running = snapshot.result(timeout=5)['live_instrument']
            self.assertEqual((running['name'], running['version']), ('old', 'loaded-version'))
            self.assertEqual(activation.result(timeout=5)[0], 200)
        self.assertEqual(self.details()['live_instrument']['name'], 'new')

    def test_upload_filename_normalization_matches_existing_contract(self):
        for name, expected in (('Café.zip', 'Cafe'), ('a  b.zip', 'a_b'),
                (r'a\b.zip', 'ab'), ('a @ b.zip', 'a__b'), ('../résumé.zip', 'resume')):
            with self.subTest(name=name):
                self.assertEqual(self.upload(archive(), name)[0], 200)
                self.assertTrue((self.store / (expected + '.zip')).exists())
                self.assertIn(expected, [item['name'] for item in self.details()['instruments']])
    def test_invalid_loaded_utf8_is_unknown(self):
        (self.live / 'version').write_bytes(b'\xff')
        self.assertIsNone(json.loads(self.request('/api/instruments/details')[1])['live_instrument'])

    def test_archive_metadata_replaces_invalid_utf8(self):
        self.package.write_bytes(archive(extra={'version': b'v\xff'}))
        self.stop_api()
        self.start_api()
        instruments = json.loads(self.request('/api/instruments/details')[1])['instruments']
        self.assertEqual(next(item for item in instruments if item['name'] == 'new')['version'], 'v\ufffd')

    def test_malformed_metadata_uses_utf8_replacement_semantics(self):
        for payload in (b'v\xe2\x82', b'v\xe2\x82A', b'v\xed\xa0\x80'):
            with self.subTest(payload=payload):
                self.assertEqual(self.upload(archive(extra={'version': payload}))[0], 200)
                version = next(item['version'] for item in self.details()['instruments'] if item['name'] == 'new')
                self.assertEqual(version, payload.decode(errors='replace'))

    def setUp(self):
        super().setUp()
        self.start_api()

    def details(self):
        status, body, _ = self.request('/api/instruments/details')
        self.assertEqual(status, 200)
        return json.loads(body)

    def assert_upload_preserved(self, data, expected=400):
        previous = self.package.read_bytes()
        before = self.details()['instruments']
        self.assertEqual(self.upload(data)[0], expected)
        self.assertEqual(self.package.read_bytes(), previous)
        self.assertEqual(self.details()['instruments'], before)
        self.assertEqual(list(self.store.glob('.upload-*')), [])

    def test_status_uses_loaded_identity_and_version(self):
        data = self.details()
        self.assertEqual(data['live_instrument'], {'name': 'old', 'version': 'loaded-version', 'is_default': True})
        self.assertTrue(all(item['version'] != 'loaded-version' for item in data['instruments']))
        self.assertEqual(json.loads(self.request('/api/instruments')[1])['live_instrument'], 'old')

    def test_service_exit_clears_live_status(self):
        self.assertIsNotNone(self.details()['live_instrument'])
        self.save_state(active=False)
        self.assertIsNone(self.details()['live_instrument'])

    def test_missing_identity_does_not_guess_default(self):
        (self.live / '.instrument-name').unlink()
        self.assertIsNone(self.details()['live_instrument'])

    def test_invalid_identity_is_not_reported(self):
        (self.live / '.instrument-name').write_text('../old\n')
        self.assertIsNone(self.details()['live_instrument'])

    def test_missing_version_reports_unknown(self):
        (self.live / 'version').unlink()
        self.assertIsNone(self.details()['live_instrument'])

    def test_api_restart_recovers_identity(self):
        self.stop_api(); self.start_api()
        self.assertEqual(self.details()['live_instrument']['name'], 'old')

    def test_upload_invalid_zip_preserves_archive(self): self.assert_upload_preserved(b'not a zip')
    def test_upload_missing_version_preserves_archive(self): self.assert_upload_preserved(archive(version=None))
    def test_upload_bad_crc_preserves_archive(self):
        self.assert_upload_preserved(archive().replace(b'new executable', b'bad executable'))

    def test_valid_upload_atomically_replaces_version(self):
        data = archive(version='3', compression=zipfile.ZIP_DEFLATED)
        self.assertEqual(self.upload(data)[0], 200)
        self.assertEqual(self.package.read_bytes(), data)
        self.assertEqual(next(i for i in self.details()['instruments'] if i['name'] == 'new')['version'], '3')
        self.assertEqual(list(self.store.glob('.upload-*')), [])

    def test_upload_does_not_change_loaded_version(self):
        self.assertEqual(self.upload(archive(version='4'), 'old.zip')[0], 200)
        self.assertEqual(self.details()['live_instrument']['version'], 'loaded-version')

    def test_truncated_multipart_preserves_archive(self):
        previous = self.package.read_bytes()
        data = b'--boundary\r\nContent-Disposition: form-data; name="new.zip"; filename="new.zip"\r\n\r\n' + archive()
        self.assertEqual(self.request('/api/instruments/upload', data, {'Content-Type': 'multipart/form-data; boundary=boundary'})[0], 400)
        self.assertEqual(self.package.read_bytes(), previous)
        self.assertEqual(list(self.store.glob('.upload-*')), [])

    def test_disconnected_upload_is_cleaned_up(self):
        previous = self.package.read_bytes()
        with socket.create_connection(('127.0.0.1', self.port)) as connection:
            connection.sendall(b'POST /api/instruments/upload HTTP/1.1\r\nHost: localhost\r\nContent-Length: 99999\r\n'
                b'Content-Type: multipart/form-data; boundary=boundary\r\n\r\n--boundary\r\n'
                b'Content-Disposition: form-data; name="new.zip"; filename="new.zip"\r\n\r\npartial upload')
        import time
        time.sleep(0.05)
        self.assertEqual(self.package.read_bytes(), previous)
        self.assertEqual(list(self.store.glob('.upload-*')), [])

    def test_default_cannot_be_deleted(self):
        self.assertIn(b'Default instrument cannot be removed', self.request('/api/instruments/delete/old')[1])
        self.assertTrue((self.store / 'old.zip').exists())

    def test_delete_removes_archive_and_inventory(self):
        self.assertEqual(self.request('/api/instruments/delete/new')[0], 200)
        self.assertFalse(self.package.exists())
        self.assertNotIn('new', [item['name'] for item in self.details()['instruments']])

    def test_commands_download_and_missing_file(self):
        self.assertEqual(self.upload(archive(extra={'drivers.json': b'[{"class":"Common"}]'}))[0], 200)
        status, body, headers = self.request('/api/instruments/commands/new')
        self.assertEqual((status, body), (200, b'[{"class":"Common"}]'))
        self.assertIn('new-drivers.json', headers['Content-Disposition'])
        self.assertEqual(self.request('/api/instruments/commands/old')[0], 404)
        self.assertEqual(self.request('/api/instruments/commands/absent')[0], 404)

    def test_successful_run_changes_loaded_identity(self):
        self.assertEqual(self.request('/api/instruments/run/new')[0], 200)
        self.assertEqual(self.details()['live_instrument'], {'name': 'new', 'version': '2', 'is_default': False})

    def test_failed_run_reports_restored_instrument(self):
        self.save_state(fail_new=True)
        self.assertEqual(self.request('/api/instruments/run/new')[0], 500)
        self.assertEqual(self.details()['live_instrument']['name'], 'old')

    def test_failed_rollback_reports_stopped(self):
        self.save_state(fail_new=True, fail_old=True)
        self.assertEqual(self.request('/api/instruments/run/new')[0], 500)
        self.assertIsNone(self.details()['live_instrument'])

    def test_missing_instrument_and_method(self):
        self.assertEqual(self.request('/api/instruments/run/absent')[0], 404)
        self.assertEqual(self.request('/api/instruments', b'', method='POST')[0], 405)

    def test_manifest_release_and_raw_routes(self):
        (self.root / 'manifest').write_text('# comment\nboard=red-pitaya\nkernel=6.18\nkey=value=rest\n')
        (self.root / 'release').write_text('NAME=Koheron\n')
        self.assertEqual(json.loads(self.request('/api/system/manifest')[1])['key'], 'value=rest')
        self.assertEqual(json.loads(self.request('/api/system/release')[1]), {'NAME': 'Koheron'})
        combined = json.loads(self.request('/api/system/build')[1])
        self.assertEqual(combined['manifest']['board'], 'red-pitaya')
        self.assertEqual(self.request('/api/system/manifest/raw')[1], (self.root / 'manifest').read_bytes())
        self.assertEqual(self.request('/api/system/release/raw')[1], (self.root / 'release').read_bytes())

    def test_missing_system_files(self):
        for endpoint in ('manifest', 'release', 'manifest/raw', 'release/raw'):
            self.assertEqual(self.request('/api/system/' + endpoint)[0], 404)
        self.assertEqual(json.loads(self.request('/api/system/build')[1]), {'manifest': {}, 'release': {}})

    def test_log_routes_and_invalid_limits(self):
        self.assertEqual(self.request('/api/logs/koheron?lines=bad')[0], 400)
        for suffix in ('/incr', '/bookmark', '/instrument/incr', '/instrument/bookmark'):
            status, body, _ = self.request('/api/logs/koheron' + suffix)
            self.assertEqual(status, 200)
            self.assertIn('cursor', json.loads(body))

    def test_parallel_readers_keep_valid_responses(self):
        with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
            for result in pool.map(lambda _: self.details(), range(32)):
                self.assertEqual(result['live_instrument']['name'], 'old')


if __name__ == '__main__': unittest.main()
