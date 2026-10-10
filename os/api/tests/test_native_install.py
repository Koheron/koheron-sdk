import io
import shutil
import stat
import unittest
import zipfile

from native_fixture import NativeFixture, archive


class NativeInstallTest(NativeFixture):
    def test_invalid_version_encoding_does_not_stop(self):
        self.package.write_bytes(archive(extra={'version': b'\xff'}))
        self.assertNotEqual(self.install().returncode, 0)
        self.assert_old()
        self.assertFalse(any(line.startswith('stop ') for line in self.operations()))

    def test_success_preserves_executable_permissions(self):
        result = self.install()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((self.live / 'serverd').read_bytes(), b'new executable')
        self.assertTrue((self.live / 'serverd').stat().st_mode & stat.S_IXUSR)
        self.assertTrue(self.load_state()['active'])
        self.assertEqual(list(self.root.glob('.instrument-*')), [])

    def reject_before_stop(self, data):
        self.package.write_bytes(data)
        self.assertNotEqual(self.install().returncode, 0)
        self.assertEqual(self.operations(), [])
        self.assert_old()
        self.assertEqual(list(self.root.glob('.instrument-*')), [])

    def test_invalid_zip_does_not_stop(self): self.reject_before_stop(b'broken zip')
    def test_crc_failure_does_not_stop(self): self.reject_before_stop(archive().replace(b'bitstream', b'corrupted'))
    def test_missing_version_does_not_stop(self): self.reject_before_stop(archive(version=None))
    def test_empty_version_does_not_stop(self): self.reject_before_stop(archive(version=' \n'))
    def test_non_executable_does_not_stop(self): self.reject_before_stop(archive(executable=False))
    def test_traversal_does_not_stop(self): self.reject_before_stop(archive(extra={'../escaped': b'bad'}))
    def test_absolute_path_does_not_stop(self): self.reject_before_stop(archive(extra={'/escaped': b'bad'}))
    def test_backslash_does_not_stop(self): self.reject_before_stop(archive(extra={'..\\escaped': b'bad'}))
    def test_identity_override_does_not_stop(self): self.reject_before_stop(archive(extra={'.instrument-name': b'bad'}))

    def test_symlink_member_does_not_stop(self):
        stream = io.BytesIO(archive())
        with zipfile.ZipFile(stream, 'a') as package:
            link = zipfile.ZipInfo('link'); link.external_attr = (stat.S_IFLNK | 0o777) << 16
            package.writestr(link, '../escaped')
        self.reject_before_stop(stream.getvalue())

    def test_duplicate_normalized_member_does_not_stop(self):
        self.reject_before_stop(archive(extra={'./version': b'other'}))

    def test_missing_fpga_payload_does_not_stop(self):
        output = io.BytesIO()
        with zipfile.ZipFile(output, 'w') as package:
            package.writestr('version', '2')
            info = zipfile.ZipInfo('serverd'); info.external_attr = (stat.S_IFREG | 0o755) << 16
            package.writestr(info, 'executable')
        self.reject_before_stop(output.getvalue())

    def test_legacy_payload_is_supported(self):
        self.package.write_bytes(archive(legacy=True))
        self.assertEqual(self.install().returncode, 0)

    def test_start_failure_restores_previous(self):
        self.save_state(fail_new=True)
        self.assertNotEqual(self.install().returncode, 0)
        self.assert_old()
        self.assertTrue(self.load_state()['active'])
        self.assertEqual(list(self.root.glob('.instrument-*')), [])

    def test_stop_failure_preserves_previous(self):
        self.save_state(fail_stop=True)
        self.assertNotEqual(self.install().returncode, 0)
        self.assert_old(); self.assertTrue(self.load_state()['active'])

    def test_failed_previous_restart_reports_stopped(self):
        self.save_state(fail_new=True, fail_old=True)
        self.assertNotEqual(self.install().returncode, 0)
        self.assert_old(); self.assertFalse(self.load_state()['active'])

    def test_inactive_previous_is_not_started_after_failure(self):
        self.save_state(active=False, fail_new=True)
        self.assertNotEqual(self.install().returncode, 0)
        self.assert_old(); self.assertFalse(self.load_state()['active'])
        self.assertEqual(self.operations().count('start koheron-server.service'), 1)

    def test_first_install_failure_leaves_no_live_directory(self):
        shutil.rmtree(self.live); self.save_state(active=False, fail_new=True)
        self.assertNotEqual(self.install().returncode, 0)
        self.assertFalse(self.live.exists()); self.assertFalse(self.load_state()['active'])

    def test_led_failure_preserves_success(self):
        self.save_state(fail_led=True)
        result = self.install()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue(self.load_state()['active'])
        self.assertEqual((self.live / '.instrument-name').read_text().strip(), 'new')


if __name__ == '__main__': unittest.main()
