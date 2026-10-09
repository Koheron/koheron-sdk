"""Exercise checked writes, including kernel-style errors hidden by stdio."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class FpgaLoadTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        temporary = tempfile.TemporaryDirectory()
        cls.addClassCleanup(temporary.cleanup)
        cls.binary = Path(temporary.name) / 'fpga-load'
        subprocess.run([
            os.environ.get('CXX', 'g++'), '-std=c++23', '-O2', '-Wall', '-Wextra',
            '-Werror', '-Wpedantic', '-fno-exceptions', '-I', str(ROOT),
            str(ROOT / 'server/tests/fpga_load.cpp'), '-o', str(cls.binary),
        ], check=True, timeout=60)

    def invoke(self, action, path, value, success):
        result = subprocess.run([str(self.binary), action, str(path), value],
                                capture_output=True, text=True, timeout=5)
        self.assertEqual(result.returncode, 0 if success else 1, result.stderr)

    def test_write_preserves_command_and_does_not_create_missing_attribute(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'firmware'
            self.invoke('write', path, 'test.bit.bin\n', False)
            self.assertFalse(path.exists())
            path.touch()
            self.invoke('write', path, 'test.bit.bin\n', True)
            self.assertEqual(path.read_text(), 'test.bit.bin\n')

    @unittest.skipUnless(Path('/dev/full').exists(), 'requires Linux /dev/full')
    def test_write_error_is_reported_without_stdio_buffering(self):
        self.invoke('write', '/dev/full', 'test.bit.bin\n', False)

    def test_exact_state_match_and_unreadable_state(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'state'
            self.invoke('state', path, 'operating', False)
            for value, expected in [('operating\n', True), ('operating: 0x1\n', False),
                                    ('write error\n', False), ('', False)]:
                path.write_text(value)
                self.invoke('state', path, 'operating', expected)

    def test_false_applied_status_from_failed_configfs_overlay_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            overlay = Path(directory)
            (overlay / 'status').write_text('applied\n')
            (overlay / 'path').write_text('\n')
            self.invoke('overlay', overlay, 'pl.dtbo', False)
            (overlay / 'path').write_text('pl.dtbo\n')
            self.invoke('overlay', overlay, 'pl.dtbo', True)
            (overlay / 'status').write_text('unapplied\n')
            self.invoke('overlay', overlay, 'pl.dtbo', False)


if __name__ == '__main__':
    unittest.main()
