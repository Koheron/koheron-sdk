"""Check cached native management compile/link settings with a fake compiler."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


SDK = Path(__file__).resolve().parents[2]


class ApiMakeTest(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.output = self.root / 'out'
        self.settings = self.root / 'compiler-settings'
        self.settings.write_text('image=first\n')
        self.compiler = self.root / 'compiler.py'
        self.compiler.write_text('''import json, pathlib, sys
args = sys.argv[1:]
output = pathlib.Path(args[args.index('-o') + 1])
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(json.dumps(args))
if '-c' in args and pathlib.Path(__file__).with_name('fail-compile').exists():
    output.write_text('partial object')
    raise SystemExit(19)
with pathlib.Path(__file__).with_name('calls').open('a') as stream:
    stream.write(output.name + '\\n')
''')
        self.calls = self.root / 'calls'
        self.calls.touch()

    def make(self, *settings, success=True):
        self.calls.write_text('')
        result = subprocess.run(['make', '--no-print-directory', '-j4', '-f',
                                 str(SDK / 'os/api/Makefile'), f'BUILD_DIR={self.output}',
                                 f'CXX={sys.executable} {self.compiler}', 'PKG_CONFIG=true',
                                 f'COMPILER_SETTINGS={self.settings}', *settings,
                                 'all', 'journal-probe'],
                                text=True, capture_output=True, timeout=10,
                                env={**os.environ, 'MAKEFLAGS': '', 'MFLAGS': ''})
        self.assertEqual(result.returncode == 0, success, result.stdout + result.stderr)
        for path in self.output.glob('koheron-*'):
            self.assertFalse(any('settings' in arg for arg in json.loads(path.read_text())))
        return set(self.calls.read_text().splitlines())

    def test_compile_and_link_changes_refresh_only_affected_outputs(self):
        initial = self.make()
        self.assertIn('sha1.o', initial)
        mtimes = {p: p.stat().st_mtime_ns for p in self.output.iterdir()}
        self.assertEqual(self.make(), set())
        self.assertEqual(mtimes, {p: p.stat().st_mtime_ns for p in self.output.iterdir()})
        for flags in ('CXXFLAGS=-O0 -DFEATURE=1', 'CPPFLAGS=-DFEATURE=2'):
            with self.subTest(flags=flags):
                self.assertEqual(self.make(flags), initial)
                self.assertEqual(self.make(flags), set())
        self.make()
        links = {'koheron-api', 'koheron-install', 'koheron-server-init', 'journal-probe'}
        for flags in ('LDFLAGS=-Wl,--as-needed', 'LDLIBS=-lm'):
            with self.subTest(flags=flags):
                self.make()
                self.assertEqual(self.make(flags), links)
                self.assertEqual(self.make(flags), set())

    def test_compiler_settings_rebuild_sha1_alongside_other_objects(self):
        initial = self.make()
        self.settings.write_text('image=second\n')
        self.assertEqual(self.make(), initial)
        self.assertEqual(self.make(), set())
        self.assertEqual(self.make(f'CXX={sys.executable} -u {self.compiler}'), initial)

    def test_failed_compilation_cannot_leave_cached_partial_objects(self):
        self.make()
        binaries = {p: (p.read_bytes(), p.stat().st_mtime_ns)
                    for p in self.output.glob('koheron-*')}
        failure = self.root / 'fail-compile'
        failure.touch()
        self.make('CXXFLAGS=-O0', success=False)
        self.assertEqual(binaries, {p: (p.read_bytes(), p.stat().st_mtime_ns) for p in binaries})
        for path in self.output.glob('*.o'):
            self.assertNotEqual(path.read_text(), 'partial object')
        failure.unlink()
        self.assertIn('sha1.o', self.make('CXXFLAGS=-O0'))
        self.assertEqual(self.make('CXXFLAGS=-O0'), set())


if __name__ == '__main__':
    unittest.main()
