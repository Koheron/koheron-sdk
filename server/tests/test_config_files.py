"""Configuration I/O failures must return errors in exception-free builds."""
from pathlib import Path
import tempfile
import unittest
from cpp_fixture import compile_cpp, run_cpp


class ConfigFilesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.binary = compile_cpp(cls, 'server/tests/config_files.cpp',
            flags=('-Wpedantic', '-fno-exceptions', '-DINSTRUMENT_NAME="config-test"'))

    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)

    def run_config(self, action, path, *, success=True):
        result = run_cpp(self, self.binary, action, path, cwd=self.root, timeout=5,
                         returncode=0 if success else 1)
        if not success:
            self.assertRegex(result.stderr, r'^(generic|system|iostream):[1-9][0-9]*:')
        return result

    def test_round_trip_and_replacement(self):
        path = self.root / 'nested/config.ini'
        self.run_config('save', path)
        self.run_config('load', path)
        path.write_text('old content')
        self.run_config('save', path)
        self.run_config('load', path)
        self.assertFalse(path.with_suffix('.ini.tmp').exists())

    def test_relative_filename(self):
        self.run_config('save', 'config.ini')
        self.run_config('load', 'config.ini')

    def test_missing_input(self):
        self.run_config('load', self.root / 'missing.ini', success=False)

    def test_read_failure(self):
        # Opening a directory succeeds on Linux; reading it reports an I/O error.
        self.run_config('load', self.root, success=False)


    def test_manager_checked_save_round_trip_and_retry(self):
        path = self.root / 'nested/config.ini'
        self.run_config('manager-save', path)
        self.run_config('load', path)
        original = path.read_bytes()
        temporary = path.with_suffix('.ini.tmp')
        temporary.mkdir()
        self.run_config('manager-save', path, success=False)
        self.assertEqual(path.read_bytes(), original)
        temporary.rmdir()
        self.run_config('manager-save', path)
        self.run_config('load', path)
        self.assertFalse(temporary.exists())

    def test_manager_checked_save_reports_directory_flush_and_rename_failures(self):
        parent = self.root / 'parent'
        parent.write_text('preserve')
        self.run_config('manager-save', parent / 'config.ini', success=False)
        self.assertEqual(parent.read_text(), 'preserve')
        path = self.root / 'config.ini'
        path.write_text('preserve')
        path.with_suffix('.ini.tmp').symlink_to('/dev/full')
        self.run_config('manager-save', path, success=False)
        self.assertEqual(path.read_text(), 'preserve')
        destination = self.root / 'destination'
        destination.mkdir()
        (destination / 'keep').write_text('preserve')
        self.run_config('manager-save', destination, success=False)
        self.assertEqual((destination / 'keep').read_text(), 'preserve')

    def test_manager_legacy_save_logs_failure(self):
        path = self.root / 'config.ini'
        path.write_text('preserve')
        path.with_suffix('.ini.tmp').mkdir()
        result = self.run_config('manager-legacy', path)
        self.assertIn('Configuration save failed', result.stderr)
        self.assertEqual(path.read_text(), 'preserve')


if __name__ == '__main__':
    unittest.main()
