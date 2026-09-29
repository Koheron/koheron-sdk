"""Live identity follows extracted files and service state, not boot preference."""
import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
import types
import unittest
from unittest.mock import Mock, patch
import zipfile

import flask  # Keep framework imports outside the temporary systemd stub.


class LiveStatusTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        journal = Mock()
        journal.Reader.return_value.get_previous.return_value = None
        spec = importlib.util.spec_from_file_location('status_api', Path(__file__).parents[1] / '__init__.py')
        cls.module = importlib.util.module_from_spec(spec)
        with patch.dict(sys.modules, {'systemd': types.SimpleNamespace(journal=journal),
                                      'status_api': cls.module}), \
             patch('os.listdir', return_value=[]), patch('subprocess.run'):
            spec.loader.exec_module(cls.module)

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.store = self.root / 'store'
        self.live = self.root / 'live'
        self.store.mkdir()
        self.live.mkdir()
        for name in ('default', 'selected'):
            with zipfile.ZipFile(self.store / (name + '.zip'), 'w') as archive:
                archive.writestr('version', 'archive-version')
        (self.store / 'default').write_text('default.zip')
        (self.live / '.instrument-name').write_text('selected\n')
        (self.live / 'version').write_text('loaded-version')
        self.active = True
        def service(*args, **kwargs):
            return subprocess.CompletedProcess([], 0 if self.active else 3)
        run = patch('subprocess.run', side_effect=service)
        self.run = run.start()
        self.addCleanup(run.stop)
        for attr, value in [('instruments_dirname', str(self.store)),
                            ('live_instrument_dirname', str(self.live))]:
            p = patch.object(self.module.KoheronApp, attr, value)
            p.start()
            self.addCleanup(p.stop)
        self.app = self.module.KoheronApp('status-test')
        # Routes were registered on the global app at module load.
        global_app = self.module.app
        global_app.instruments_dirname = str(self.store)
        global_app.live_instrument_dirname = str(self.live)
        global_app.init_instruments(str(self.store))
        global_app.config['TESTING'] = True
        self.client = global_app.test_client()

    def test_api_restart_recovers_selected_instrument(self):
        self.assertEqual(self.app.live_instrument,
                         {'name': 'selected', 'version': 'loaded-version', 'is_default': False})

    def test_status_uses_loaded_version_not_uploaded_version(self):
        data = self.client.get('/api/instruments/details').json
        self.assertEqual(data['live_instrument']['version'], 'loaded-version')
        self.assertTrue(all(item['version'] == 'archive-version' for item in data['instruments']))

    def test_service_exit_clears_live_status(self):
        self.assertEqual(self.client.get('/api/instruments').json['live_instrument'], 'selected')
        self.active = False
        self.assertIsNone(self.client.get('/api/instruments').json['live_instrument'])

    def test_no_identity_does_not_guess_default(self):
        (self.live / '.instrument-name').unlink()
        self.assertIsNone(self.client.get('/api/instruments').json['live_instrument'])

    def test_missing_version_reports_unknown(self):
        (self.live / 'version').unlink()
        self.assertIsNone(self.client.get('/api/instruments/details').json['live_instrument'])

    def test_failed_start_reports_stopped(self):
        def failed_install(*args):
            self.active = False
            return 1
        with patch('subprocess.call', side_effect=failed_install):
            self.assertEqual(self.client.get('/api/instruments/run/default').status_code, 500)
        self.assertIsNone(self.client.get('/api/instruments').json['live_instrument'])

    def test_rollback_reports_restored_instrument(self):
        with patch('subprocess.call', return_value=1):
            self.assertEqual(self.client.get('/api/instruments/run/default').status_code, 500)
        self.assertEqual(self.client.get('/api/instruments').json['live_instrument'], 'selected')

    def test_success_reads_installed_identity(self):
        def successful_install(*args):
            (self.live / '.instrument-name').write_text('default\n')
            (self.live / 'version').write_text('new-loaded-version')
            return 0
        with patch('subprocess.call', side_effect=successful_install):
            self.assertEqual(self.client.get('/api/instruments/run/default').status_code, 200)
        info = self.client.get('/api/instruments/details').json['live_instrument']
        self.assertEqual(info, {'name': 'default', 'version': 'new-loaded-version', 'is_default': True})

    def test_service_query_timeout_reports_unknown(self):
        self.run.side_effect = subprocess.TimeoutExpired('mock systemctl', 5)
        self.assertIsNone(self.client.get('/api/instruments').json['live_instrument'])


if __name__ == '__main__':
    unittest.main()
