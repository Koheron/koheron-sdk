"""Exercise firmware preparation with real files instead of board sysfs nodes."""
import os
from pathlib import Path
import tempfile
import unittest
from cpp_fixture import compile_cpp, run_cpp


class FirmwareFilesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.binary = compile_cpp(cls, 'server/tests/firmware_files.cpp',
            flags=('-Wpedantic', '-fno-exceptions'))

    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.root = Path(tmp.name)
        self.live = self.root / 'live-instrument'
        self.live.mkdir()
        self.firmware = self.root / 'firmware'
        self.parameter = self.root / 'path'
        self.parameter.write_text('\n')
        self.payloads = {'pl.dtbo': b'new overlay', 'test.bit.bin': b'new bitstream'}
        for name, content in self.payloads.items():
            (self.live / name).write_bytes(content)

    def prepare(self, *, success=True):
        run_cpp(self, self.binary, self.live, self.firmware, self.parameter,
                timeout=5, returncode=0 if success else 1)

    def assert_copied(self):
        for name, content in self.payloads.items():
            self.assertEqual((self.firmware / name).read_bytes(), content)

    def test_unset_search_path_uses_live_files_without_copying(self):
        self.prepare()
        self.assertEqual(self.parameter.read_text(), str(self.live))
        self.assertFalse(self.firmware.exists())

    def test_existing_live_path_preserves_parameter_and_stale_firmware(self):
        configured = str(self.live) + '/\n'
        self.parameter.write_text(configured)
        self.firmware.mkdir()
        (self.firmware / 'test.bit.bin').write_bytes(b'old bitstream')
        self.prepare()
        self.assertEqual(self.parameter.read_text(), configured)
        self.assertEqual((self.firmware / 'test.bit.bin').read_bytes(), b'old bitstream')
        self.assertFalse((self.firmware / 'pl.dtbo').exists())

    def test_instrument_switch_and_rollback_keep_live_path(self):
        self.prepare()
        backup = self.root / 'previous'
        self.live.rename(backup)
        self.live.mkdir()
        for name in self.payloads:
            (self.live / name).write_bytes(b'replacement')
        self.prepare()
        self.assertEqual((Path(self.parameter.read_text()) / 'test.bit.bin').read_bytes(), b'replacement')
        replacement = self.root / 'replacement'
        self.live.rename(replacement)
        backup.rename(self.live)
        self.prepare()
        self.assertEqual((Path(self.parameter.read_text()) / 'test.bit.bin').read_bytes(), b'new bitstream')
        self.assertFalse(self.firmware.exists())

    def test_missing_parameter_copies_payloads(self):
        self.parameter.unlink()
        self.prepare()
        self.assert_copied()
        self.assertFalse(self.parameter.exists())

    @unittest.skipIf(os.geteuid() == 0, 'root bypasses file write permissions')
    def test_unwritable_parameter_copies_payloads(self):
        self.parameter.chmod(0o444)
        self.prepare()
        self.assert_copied()
        self.assertEqual(self.parameter.read_text(), '\n')

    def test_custom_search_path_is_preserved(self):
        configured = str(self.root / 'custom-firmware') + '\n'
        self.parameter.write_text(configured)
        self.prepare()
        self.assert_copied()
        self.assertEqual(self.parameter.read_text(), configured)

    def test_copy_fallback_overwrites_stale_payloads(self):
        self.parameter.unlink()
        self.firmware.mkdir()
        for name in self.payloads:
            (self.firmware / name).write_bytes(b'old')
        self.prepare()
        self.assert_copied()

    def test_missing_or_empty_payload_does_not_enable_stale_firmware(self):
        for name, content in self.payloads.items():
            for empty in (False, True):
                with self.subTest(name=name, empty=empty):
                    source = self.live / name
                    if empty:
                        source.write_bytes(b'')
                    else:
                        source.unlink()
                    self.prepare(success=False)
                    self.assertEqual(self.parameter.read_text(), '\n')
                    self.assertFalse(self.firmware.exists())
                    source.write_bytes(content)

    def test_copy_error_is_reported(self):
        self.parameter.unlink()
        self.firmware.write_bytes(b'not a directory')
        self.prepare(success=False)
        self.assertEqual(self.firmware.read_bytes(), b'not a directory')


if __name__ == '__main__':
    unittest.main()
