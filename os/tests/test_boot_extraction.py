"""Run boot extraction against real archives without accessing board paths."""
from pathlib import Path
import stat
import subprocess
import tempfile
import unittest
import zipfile


SCRIPT = Path(__file__).resolve().parents[1] / 'scripts/unzip_default_instrument.sh'


class BootExtractionTest(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.instruments = self.root / 'instruments'
        self.instruments.mkdir()
        self.live = self.root / 'live'
        self.archive = self.instruments / 'example.zip'
        (self.instruments / 'default').write_text('example.zip\n')

    def package(self, legacy=False, reference=True):
        members = {'serverd': b'executable', 'version': b'1',
                   'index.html': b'web assets', 'drivers.json': b'[]'}
        if legacy or reference:
            members['example.bit'] = b'original bitstream'
        if not legacy:
            members.update({'example.bit.bin': b'runtime bitstream',
                            'pl.dtbo': b'overlay'})
        with zipfile.ZipFile(self.archive, 'w', zipfile.ZIP_DEFLATED) as archive:
            for name, data in members.items():
                info = zipfile.ZipInfo(name)
                mode = 0o755 if name == 'serverd' else 0o644
                info.external_attr = (stat.S_IFREG | mode) << 16
                archive.writestr(info, data, compress_type=zipfile.ZIP_DEFLATED)

    def extract(self, loader):
        return subprocess.run(['bash', str(SCRIPT), str(self.instruments),
                               str(self.live), loader],
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


if __name__ == '__main__':
    unittest.main()
