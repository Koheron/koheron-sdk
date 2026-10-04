"""Exercise real archive/file operations with all systemctl calls mocked."""
import importlib.util
from pathlib import Path
import stat
import struct
import subprocess
import tempfile
import unittest
from unittest.mock import patch
import zipfile

spec = importlib.util.spec_from_file_location('installer', Path(__file__).parents[1] / 'install_instrument.py')
installer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(installer)


class InstallTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.live = self.root / 'live'
        self.live.mkdir()
        (self.live / '.instrument-name').write_text('old\n')
        (self.live / 'version').write_text('1')
        (self.live / 'serverd').write_text('old executable')
        self.archive = self.root / 'new.zip'
        self.package()
        self.active = True
        self.fail_new = False
        self.fail_old = False
        self.fail_stop = False
        self.fail_led = False
        self.operations = []
        self.mock = patch.object(installer, 'systemctl', side_effect=self.service)
        self.mock.start()
        self.addCleanup(self.mock.stop)

    def package(self, extra=None, executable=True, legacy=False):
        members = {'version': b'2', 'serverd': b'new executable',
                   **({'new.bit': b'bitstream'} if legacy else
                      {'pl.dtbo': b'overlay', 'new.bit.bin': b'bitstream'})}
        if extra:
            members.update(extra)
        with zipfile.ZipFile(self.archive, 'w') as z:
            for name, data in members.items():
                info = zipfile.ZipInfo(name)
                info.external_attr = (stat.S_IFREG | (0o755 if name == 'serverd' and executable else 0o644)) << 16
                z.writestr(info, data)

    def service(self, action, unit=installer.SERVICE, check=True):
        self.operations.append((action, unit))
        if unit != installer.SERVICE:
            if self.fail_led:
                raise subprocess.CalledProcessError(1, ['mock systemctl'])
            return subprocess.CompletedProcess([], 0)
        if action == 'is-active':
            return subprocess.CompletedProcess([], 0 if self.active else 3)
        if action == 'stop':
            if self.fail_stop:
                raise subprocess.CalledProcessError(1, ['mock stop'])
            self.active = False
        if action == 'start':
            name = (self.live / '.instrument-name').read_text().strip()
            if (name == 'new' and self.fail_new) or (name == 'old' and self.fail_old):
                raise subprocess.CalledProcessError(1, ['mock start'])
            self.active = True
        return subprocess.CompletedProcess([], 0)

    def assert_old(self):
        self.assertEqual((self.live / 'serverd').read_text(), 'old executable')
        self.assertEqual((self.live / '.instrument-name').read_text().strip(), 'old')

    def test_success(self):
        installer.install(self.archive, self.live)
        self.assertEqual((self.live / 'serverd').read_text(), 'new executable')
        self.assertEqual((self.live / '.instrument-name').read_text().strip(), 'new')
        self.assertTrue((self.live / 'serverd').stat().st_mode & stat.S_IXUSR)
        self.assertTrue(self.active)
        self.assertEqual(list(self.root.glob('.instrument-*')), [])

    def test_invalid_archive_does_not_stop_service(self):
        self.archive.write_bytes(b'broken zip')
        with self.assertRaises(zipfile.BadZipFile):
            installer.install(self.archive, self.live)
        self.assert_old()
        self.assertEqual(self.operations, [])

    def test_corrupt_member_does_not_stop_service(self):
        # Keep the ZIP readable but corrupt its last payload, so extraction has
        # already staged other files when the CRC check fails.
        with zipfile.ZipFile(self.archive) as archive:
            member = archive.getinfo('new.bit.bin')
            self.assertEqual(member.compress_type, zipfile.ZIP_STORED)
        with self.archive.open('r+b') as archive:
            archive.seek(member.header_offset)
            header = archive.read(30)
            name_size, extra_size = struct.unpack_from('<HH', header, 26)
            archive.seek(member.header_offset + 30 + name_size + extra_size)
            byte = archive.read(1)
            archive.seek(-1, 1)
            archive.write(bytes([byte[0] ^ 1]))

        with self.assertRaisesRegex(zipfile.BadZipFile, 'Bad CRC-32'):
            installer.install(self.archive, self.live)
        self.assert_old()
        self.assertEqual((self.live / 'version').read_text(), '1')
        self.assertTrue(self.active)
        self.assertEqual(self.operations, [])
        self.assertEqual(list(self.root.glob('.instrument-*')), [])

    def test_missing_payload_does_not_stop_service(self):
        with zipfile.ZipFile(self.archive, 'w') as z:
            z.writestr('version', '2')
        with self.assertRaises(ValueError):
            installer.install(self.archive, self.live)
        self.assert_old()
        self.assertEqual(self.operations, [])

    def test_non_executable_rejected_before_stop(self):
        self.package(executable=False)
        with self.assertRaises(ValueError):
            installer.install(self.archive, self.live)
        self.assertEqual(self.operations, [])
        self.assert_old()

    def test_archive_cannot_write_outside_staging(self):
        self.package(extra={'../escaped': b'bad'})
        with self.assertRaises(ValueError):
            installer.install(self.archive, self.live)
        self.assertFalse((self.root / 'escaped').exists())
        self.assertEqual(self.operations, [])

    def test_legacy_payload(self):
        self.package(legacy=True)
        installer.install(self.archive, self.live)
        self.assertTrue(self.active)

    def test_start_failure_restores_and_restarts_previous(self):
        self.fail_new = True
        with self.assertRaises(subprocess.CalledProcessError):
            installer.install(self.archive, self.live)
        self.assert_old()
        self.assertTrue(self.active)
        self.assertEqual(list(self.root.glob('.instrument-*')), [])

    def test_stop_failure_preserves_live_directory(self):
        self.fail_stop = True
        with self.assertRaises(subprocess.CalledProcessError):
            installer.install(self.archive, self.live)
        self.assert_old()

    def test_rename_failure_restores_previous(self):
        real_rename = Path.rename
        def fail_staged(path, target):
            if path.name == 'next':
                raise OSError('simulated rename failure')
            return real_rename(path, target)
        with patch.object(Path, 'rename', fail_staged):
            with self.assertRaises(OSError):
                installer.install(self.archive, self.live)
        self.assert_old()
        self.assertTrue(self.active)

    def test_failed_rollback_keeps_backup(self):
        self.fail_new = True
        real_rename = Path.rename
        def fail_restore(path, target):
            if path.name == 'previous':
                raise OSError('simulated restore failure')
            return real_rename(path, target)
        with patch.object(Path, 'rename', fail_restore):
            with self.assertRaises(OSError):
                installer.install(self.archive, self.live)
        backups = list(self.root.glob('.instrument-*/previous/serverd'))
        self.assertEqual(len(backups), 1)
        self.assertEqual(backups[0].read_text(), 'old executable')
        self.assertFalse(self.active)

    def test_previous_service_cannot_restart(self):
        self.fail_new = self.fail_old = True
        with self.assertRaises(subprocess.CalledProcessError):
            installer.install(self.archive, self.live)
        self.assert_old()
        self.assertFalse(self.active)

    def test_inactive_previous_not_started_on_failure(self):
        self.active = False
        self.fail_new = True
        with self.assertRaises(subprocess.CalledProcessError):
            installer.install(self.archive, self.live)
        self.assert_old()
        self.assertFalse(self.active)

    def test_first_install_failure_leaves_no_live_directory(self):
        import shutil
        shutil.rmtree(self.live)
        self.active = False
        self.fail_new = True
        with self.assertRaises(subprocess.CalledProcessError):
            installer.install(self.archive, self.live)
        self.assertFalse(self.live.exists())
        self.assertFalse(self.active)

    def test_led_failure_does_not_undo_success(self):
        self.fail_led = True
        installer.install(self.archive, self.live)
        self.assertEqual((self.live / '.instrument-name').read_text().strip(), 'new')
        self.assertTrue(self.active)


if __name__ == '__main__':
    unittest.main()
