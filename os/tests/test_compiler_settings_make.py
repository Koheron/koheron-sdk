"""Compiler stamps survive overlapping builds and failed image inspection."""
import os
from pathlib import Path
import subprocess
import tempfile
import time
import unittest

SDK = Path(__file__).resolve().parents[2]


class CompilerSettingsPublicationTest(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.bin = self.root / 'bin'
        self.bin.mkdir()
        self.target = self.root / 'cache/.compiler-settings'
        makefile = self.root / 'Makefile'
        makefile.write_text(f'''\
SHELL := /bin/bash
.SHELLFLAGS := -eu -o pipefail -c
.DELETE_ON_ERROR:
GCC_VERSION := 15
DOCKER_IMAGE := fixture
include {SDK}/os/compiler-settings.mk
$(eval $(call compiler_settings_stamp,{self.target},arm-linux-gnueabihf-gcc-15))
''')
        docker = self.bin / 'docker'
        docker.write_text('''\
#!/usr/bin/python3
import os, signal, sys, time
mode = os.environ.get('STAMP_INSPECTION_MODE')
if mode == 'fail':
    print('partial image ID')
    sys.exit(19)
if mode == 'term':
    os.kill(os.getppid(), signal.SIGTERM)
    time.sleep(.05)
    sys.exit(0)
print('sha256:fixture')
''')
        docker.chmod(0o755)
        self.environment = {**os.environ, 'PATH': str(self.bin) + ':' + os.environ['PATH']}
        self.argv = ['make', '--no-print-directory', '-f', str(makefile), str(self.target)]
        result = self.make()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.original = self.target.read_bytes()
        self.timestamp = self.target.stat().st_mtime_ns

    def make(self, **environment):
        return subprocess.run(self.argv, capture_output=True, text=True, timeout=10,
                              env={**self.environment, **environment})

    def assert_previous_stamp_preserved(self):
        self.assertEqual(self.target.read_bytes(), self.original)
        self.assertEqual(self.target.stat().st_mtime_ns, self.timestamp)
        self.assertEqual(list(self.target.parent.glob(self.target.name + '.tmp*')), [])

    def test_concurrent_unchanged_checks_use_separate_temporary_files(self):
        ready = self.root / 'ready'
        release = self.root / 'release'
        comparator = self.bin / 'cmp'
        comparator.write_text(f'''\
#!/usr/bin/python3
import os, sys, time
from pathlib import Path
if os.environ.get('STAMP_RUN_ID') == 'second':
    Path({str(ready)!r}).touch()
    deadline = time.monotonic() + 5
    while not Path({str(release)!r}).exists():
        if time.monotonic() > deadline:
            sys.exit(99)
        time.sleep(.01)
os.execv('/usr/bin/cmp', ['cmp', *sys.argv[1:]])
''')
        comparator.chmod(0o755)
        second = subprocess.Popen(self.argv, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                  text=True, env={**self.environment, 'STAMP_RUN_ID': 'second'})
        try:
            deadline = time.monotonic() + 5
            while not ready.exists():
                if second.poll() is not None or time.monotonic() > deadline:
                    stdout, stderr = second.communicate(timeout=1)
                    self.fail('Second check did not reach comparison: ' + stdout + stderr)
                time.sleep(.01)
            first = self.make()
            self.assertEqual(first.returncode, 0, first.stdout + first.stderr)
            release.touch()
            stdout, stderr = second.communicate(timeout=5)
            self.assertEqual(second.returncode, 0, stdout + stderr)
        finally:
            release.touch()
            if second.poll() is None:
                second.kill()
            second.communicate(timeout=5)
        self.assert_previous_stamp_preserved()

    def test_failed_inspection_preserves_stamp_and_cleans_temporary_file(self):
        result = self.make(STAMP_INSPECTION_MODE='fail')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('19', result.stderr)
        self.assert_previous_stamp_preserved()

    def test_interrupted_inspection_preserves_stamp_and_cleans_temporary_file(self):
        result = self.make(STAMP_INSPECTION_MODE='term')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('143', result.stderr)
        self.assert_previous_stamp_preserved()


if __name__ == '__main__':
    unittest.main()
