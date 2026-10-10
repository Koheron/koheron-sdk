"""Run native boot extraction against real archives without board paths/services."""
from pathlib import Path
import stat
import subprocess
import tempfile
import unittest
import zipfile

from native_fixture import BIN_DIR

class NativeBootTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not (BIN_DIR / 'koheron-install').is_file():
            raise RuntimeError('Build native binaries and set NATIVE_API_BIN_DIR before running tests')

    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.instruments = self.root / 'instruments'
        self.instruments.mkdir()
        self.live = self.root / 'live'
        self.archive = self.instruments / 'example.zip'
        (self.instruments / 'default').write_text('example.zip\n')

    def package(self, legacy=False, reference=True, compression=zipfile.ZIP_DEFLATED):
        members = {'serverd': b'executable', 'version': b'1',
                   'index.html': b'web assets', 'drivers.json': b'[]'}
        if legacy or reference:
            members['example.bit'] = b'original bitstream'
        if not legacy:
            members.update({'example.bit.bin': b'runtime bitstream',
                            'pl.dtbo': b'overlay'})
        with zipfile.ZipFile(self.archive, 'w', compression) as archive:
            for name, data in members.items():
                info = zipfile.ZipInfo(name)
                mode = 0o755 if name == 'serverd' else 0o644
                info.external_attr = (stat.S_IFREG | mode) << 16
                archive.writestr(info, data, compress_type=compression)

    def extract(self, loader):
        return subprocess.run([str(BIN_DIR / 'koheron-install'), '--extract-default',
                               '--instruments', str(self.instruments),
                               '--live', str(self.live), '--loader', loader],
                              capture_output=True, text=True, timeout=5)

    def assert_runtime_files(self):
        self.assertEqual((self.live / '.instrument-name').read_text(), 'example\n')
        self.assertEqual((self.live / 'serverd').read_bytes(), b'executable')
        self.assertTrue((self.live / 'serverd').stat().st_mode & stat.S_IXUSR)
        self.assertEqual((self.live / 'index.html').read_bytes(), b'web assets')
        self.assertEqual((self.live / 'drivers.json').read_bytes(), b'[]')

    def test_overlay_skips_reference_bitstream_and_keeps_runtime_payload(self):
        self.package()
        result = self.extract('overlay')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assert_runtime_files()
        self.assertFalse((self.live / 'example.bit').exists())
        self.assertEqual((self.live / 'example.bit.bin').read_bytes(), b'runtime bitstream')
        self.assertEqual((self.live / 'pl.dtbo').read_bytes(), b'overlay')
        with zipfile.ZipFile(self.archive) as archive:
            self.assertEqual(archive.read('example.bit'), b'original bitstream')

    def test_overlay_package_without_reference_bitstream(self):
        self.package(reference=False)
        result = self.extract('overlay')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assert_runtime_files()

    def test_xdevcfg_keeps_reference_bitstream_in_mixed_package(self):
        self.package()
        result = self.extract('xdevcfg')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assert_runtime_files()
        self.assertEqual((self.live / 'example.bit').read_bytes(), b'original bitstream')

    def test_legacy_package(self):
        self.package(legacy=True)
        result = self.extract('xdevcfg')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assert_runtime_files()
        self.assertEqual((self.live / 'example.bit').read_bytes(), b'original bitstream')

    def test_corrupt_archive_does_not_mark_instrument_live(self):
        self.archive.write_bytes(b'broken zip')
        self.assertNotEqual(self.extract('overlay').returncode, 0)
        self.assertFalse((self.live / '.instrument-name').exists())

    def test_invalid_loader_fails_before_extraction(self):
        self.package()
        self.assertNotEqual(self.extract('invalid').returncode, 0)
        self.assertFalse(self.live.exists())

    def test_replacement_removes_stale_files_and_preserves_archive(self):
        self.package()
        original = self.archive.read_bytes()
        self.live.mkdir()
        (self.live / 'stale').write_text('old payload')
        (self.live / '.instrument-name').write_text('old\n')
        self.assertEqual(self.extract('overlay').returncode, 0)
        self.assert_runtime_files()
        self.assertFalse((self.live / 'stale').exists())
        self.assertEqual(self.archive.read_bytes(), original)
        self.assertEqual(list(self.root.glob('.instrument-*')), [])

    def test_bad_archive_preserves_existing_extraction(self):
        self.live.mkdir()
        (self.live / 'serverd').write_text('old executable')
        (self.live / '.instrument-name').write_text('old\n')
        for corrupt_crc in [False, True]:
            with self.subTest(corrupt_crc=corrupt_crc):
                self.package(compression=zipfile.ZIP_STORED)
                if corrupt_crc:
                    data = self.archive.read_bytes().replace(b'original bitstream', b'corruptedbitstream')
                    self.archive.write_bytes(data)
                else:
                    with zipfile.ZipFile(self.archive, 'a') as archive:
                        archive.writestr('../escaped', b'unsafe')
                self.assertNotEqual(self.extract('overlay').returncode, 0)
                self.assertEqual((self.live / 'serverd').read_text(), 'old executable')
                self.assertEqual((self.live / '.instrument-name').read_text(), 'old\n')
                self.assertFalse((self.root / 'escaped').exists())
                self.assertEqual(list(self.root.glob('.instrument-*')), [])

    def test_invalid_default_filename_does_not_extract(self):
        self.package()
        for name in ['', '.zip', '../example.zip', '/example.zip', 'example.zip\nother.zip', 'example']:
            with self.subTest(name=name):
                (self.instruments / 'default').write_text(name + '\n')
                self.assertNotEqual(self.extract('overlay').returncode, 0)
                self.assertFalse(self.live.exists())
                self.assertEqual(list(self.root.glob('.instrument-*')), [])

    def test_missing_default_does_not_extract(self):
        self.package()
        (self.instruments / 'default').unlink()
        self.assertNotEqual(self.extract('overlay').returncode, 0)
        self.assertFalse(self.live.exists())

    def test_default_archive_symlink_is_rejected(self):
        self.package()
        external = self.root / 'outside.zip'
        self.archive.rename(external)
        self.archive.symlink_to(external)
        self.assertNotEqual(self.extract('overlay').returncode, 0)
        self.assertFalse(self.live.exists())

    def test_selected_loader_requires_usable_payload(self):
        self.package(legacy=True)
        self.assertNotEqual(self.extract('overlay').returncode, 0)
        self.assertFalse(self.live.exists())
        self.package(reference=False)
        self.assertNotEqual(self.extract('xdevcfg').returncode, 0)
        self.assertFalse(self.live.exists())
        self.assertEqual(list(self.root.glob('.instrument-*')), [])

    def test_live_symlink_is_rejected_without_changing_target(self):
        self.package()
        external = self.root / 'outside'
        external.mkdir()
        (external / 'keep').write_text('untouched')
        self.live.symlink_to(external, target_is_directory=True)
        self.assertNotEqual(self.extract('overlay').returncode, 0)
        self.assertTrue(self.live.is_symlink())
        self.assertEqual((external / 'keep').read_text(), 'untouched')
        self.assertFalse((external / 'serverd').exists())

    def test_live_directory_cannot_replace_archive_storage(self):
        self.package()
        original = self.archive.read_bytes()
        self.live = self.instruments
        self.assertNotEqual(self.extract('overlay').returncode, 0)
        self.assertEqual(self.archive.read_bytes(), original)
        self.assertEqual((self.instruments / 'default').read_text(), 'example.zip\n')
        self.assertEqual(list(self.root.glob('.instrument-*')), [])


if __name__ == '__main__':
    unittest.main()
