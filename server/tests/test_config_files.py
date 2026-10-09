"""Configuration I/O failures must return errors in exception-free builds."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class ConfigFilesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        build = tempfile.TemporaryDirectory()
        cls.addClassCleanup(build.cleanup)
        cls.binary = Path(build.name) / 'config-files'
        command = [os.environ.get('CXX', 'g++'), '-std=c++23', '-O2', '-Wall',
                   '-Wextra', '-Werror', '-Wpedantic', '-fno-exceptions',
                   '-I', str(ROOT)]
        command += shlex.split(os.environ.get('CXXFLAGS', ''))
        subprocess.run(command + [str(ROOT / 'server/tests/config_files.cpp'),
                                  '-o', str(cls.binary)], check=True, timeout=60)

    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)

    def run_config(self, action, path, *, success=True):
        result = subprocess.run([str(self.binary), action, str(path)],
                                cwd=self.root, capture_output=True, text=True, timeout=5)
        self.assertEqual(result.returncode, 0 if success else 1, result.stderr)
        if not success:
            self.assertRegex(result.stderr, r'^(generic|system|iostream):[1-9][0-9]*:')

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

    def test_parent_is_file(self):
        parent = self.root / 'parent'
        parent.write_text('preserve')
        self.run_config('save', parent / 'config.ini', success=False)
        self.assertEqual(parent.read_text(), 'preserve')

    def test_temporary_file_cannot_be_opened(self):
        path = self.root / 'config.ini'
        path.write_text('preserve')
        (self.root / 'config.ini.tmp').mkdir()
        self.run_config('save', path, success=False)
        self.assertEqual(path.read_text(), 'preserve')

    def test_flush_failure_preserves_destination(self):
        path = self.root / 'config.ini'
        path.write_text('preserve')
        (self.root / 'config.ini.tmp').symlink_to('/dev/full')
        self.run_config('save', path, success=False)
        self.assertEqual(path.read_text(), 'preserve')

    def test_rename_failure_preserves_destination(self):
        path = self.root / 'config.ini'
        path.mkdir()
        (path / 'keep').write_text('preserve')
        self.run_config('save', path, success=False)
        self.assertEqual((path / 'keep').read_text(), 'preserve')


if __name__ == '__main__':
    unittest.main()
