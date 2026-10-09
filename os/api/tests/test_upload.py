"""Local Flask tests; service calls and journal access are always mocked."""
import importlib.util
import io
import os
from pathlib import Path
import sys
import tempfile
import types
import unittest
from unittest.mock import Mock, patch
import zipfile

import flask  # Load framework modules before temporarily stubbing systemd.


def archive(version='1', **members):
    buffer = io.BytesIO()
    with zipfile.ZipFile(buffer, 'w') as z:
        if version is not None:
            z.writestr('version', version)
        for name, data in members.items():
            z.writestr(name, data)
    return buffer.getvalue()


class UploadTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        journal = Mock()
        journal.Reader.return_value.get_previous.return_value = None
        spec = importlib.util.spec_from_file_location('review_api', Path(__file__).parents[1] / '__init__.py')
        cls.module = importlib.util.module_from_spec(spec)
        with patch.dict(sys.modules, {'systemd': types.SimpleNamespace(journal=journal),
                                      'review_api': cls.module,
                                      'review_api.service_status': types.SimpleNamespace(
                                          unit_is_active=Mock(return_value=False))}), \
             patch('os.listdir', return_value=[]), \
             patch('subprocess.call'):
            spec.loader.exec_module(cls.module)

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.store = Path(self.tmp.name)
        self.app = self.module.app
        self.app.config['TESTING'] = True
        self.app.instruments_dirname = str(self.store)
        self.original = archive(serverd='old executable')
        (self.store / 'fft.zip').write_bytes(self.original)
        (self.store / 'default').write_text('fft.zip')
        self.app.init_instruments(str(self.store))
        self.inventory = list(self.app.instruments_list)
        self.client = self.app.test_client()

    def upload(self, data):
        return self.client.post('/api/instruments/upload', data={'fft.zip': (io.BytesIO(data), 'fft.zip')})

    def assert_preserved(self):
        self.assertEqual((self.store / 'fft.zip').read_bytes(), self.original)
        self.assertEqual(self.app.instruments_list, self.inventory)
        self.assertEqual(list(self.store.glob('.upload-*')), [])

    def test_non_zip_preserves_default_archive(self):
        self.assertEqual(self.upload(b'not zip').status_code, 400)
        self.assert_preserved()

    def test_missing_version_preserves_archive(self):
        self.assertEqual(self.upload(archive(version=None)).status_code, 400)
        self.assert_preserved()

    def test_bad_crc_preserves_archive(self):
        data = archive(serverd='unique payload')
        data = data.replace(b'unique payload', b'broken payload')
        self.assertEqual(self.upload(data).status_code, 400)
        self.assert_preserved()

    def test_failed_save_preserves_archive(self):
        def fail_save(storage, destination):
            destination.write(b'partial upload')
            raise OSError('simulated interrupted write')
        with patch('werkzeug.datastructures.FileStorage.save', fail_save):
            with self.assertRaises(OSError):
                self.upload(archive(version='2'))
        self.assert_preserved()

    def test_failed_replace_preserves_archive(self):
        with patch.object(self.module.os, 'replace', side_effect=OSError('replace failed')):
            with self.assertRaises(OSError):
                self.upload(archive(version='2'))
        self.assert_preserved()

    def test_valid_replacement_updates_version_atomically(self):
        replacement = archive(version='2', serverd='new executable')
        real_replace = os.replace
        def check_replace(source, target):
            self.assertEqual(Path(target).read_bytes(), self.original)
            self.assertEqual(Path(source).read_bytes(), replacement)
            self.assertEqual(Path(source).parent, Path(target).parent)
            real_replace(source, target)
        with patch.object(self.module.os, 'replace', side_effect=check_replace):
            self.assertEqual(self.upload(replacement).status_code, 200)
        self.assertEqual((self.store / 'fft.zip').read_bytes(), replacement)
        self.assertEqual(self.app.instruments_list, [{'name': 'fft', 'version': '2', 'is_default': True}])
        self.assertEqual(list(self.store.glob('.upload-*')), [])


if __name__ == '__main__':
    unittest.main()
