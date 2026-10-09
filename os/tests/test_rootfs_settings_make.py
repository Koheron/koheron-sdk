"""Check cache invalidation for rootfs settings without downloading or building."""
import hashlib
import os
from pathlib import Path
import stat
import subprocess
import tempfile
import time
import unittest


SDK = Path(__file__).resolve().parents[2]


class RootfsSettingsMakeTest(unittest.TestCase):
    def test_sync_password_is_literal_for_environment_and_legacy_alias(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            mock = root / 'sshpass'
            mock.write_text('''\
#!/usr/bin/python3
import os, sys
assert sys.argv[1] == '-e'
assert os.environ['SSHPASS'] == os.environ['EXPECTED_PASSWORD']
''')
            mock.chmod(0o755)
            cache = root / 'cache'
            harness = f'''\
SHELL := /bin/bash
.SHELLFLAGS := -eu -o pipefail -c
TMP := {cache}
OS_PATH := {SDK}/os
include {SDK}/os/rootfs.mk
'''
            skipped = [cache / 'api' / path for path in (
                'wsgi.py', 'app/__init__.py', 'app/install_instrument.sh',
                'app/install_instrument.py')]
            argv = ['make', '--no-print-directory', '-f', '-']
            for path in skipped:
                argv.extend(['-o', str(path)])
            argv.append('api_sync')
            environment = {k: v for k, v in os.environ.items()
                           if k not in ('PASSWORD', 'PASSWD', 'SSHPASS')}
            for variable in ('PASSWORD', 'PASSWD'):
                with self.subTest(variable=variable):
                    password = "spaces ' dollar $ and backslash \\"
                    result = subprocess.run(
                        argv, input=harness, text=True, capture_output=True, timeout=5,
                        env={**environment, variable: password, 'EXPECTED_PASSWORD': password,
                             'PATH': str(root) + ':' + os.environ['PATH']})
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_fresh_settings_directory_and_literal_password_invalidation(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            cache = root / 'cache'
            target = cache / 'ubuntu-base-24.04.5-base-koheron-armhf.tgz.settings.sha256'
            base = Path(str(target).removesuffix('.settings.sha256'))
            source = root / 'source.tgz'
            source.write_bytes(b'cached Ubuntu source')
            calls = root / 'builds'
            runner = root / 'docker.py'
            runner.write_text(f'''\
from pathlib import Path
import sys
Path(sys.argv[4]).write_bytes(b'configured rootfs cache')
with Path({str(calls)!r}).open('a') as log:
    log.write('build\\n')
''')
            harness = f'''\
SHELL := /bin/bash
.SHELLFLAGS := -eu -o pipefail -c
.DELETE_ON_ERROR:
TMP := {cache}
OS_PATH := {SDK}/os
UBUNTU_VERSION := 24.04.5
UBUNTU_ARCH := armhf
override ROOT_TAR_PATH := {source}
DOCKER_ROOT := python3 {runner}
.PHONY: FORCE
FORCE:
include {SDK}/os/rootfs.mk
'''
            environment = {k: v for k, v in os.environ.items()
                           if k not in ('PASSWORD', 'PASSWD', 'TIMEZONE')}

            def build(password, timezone='Europe/Paris', variable='PASSWORD'):
                result = subprocess.run(
                    ['make', '--no-print-directory', '-f', '-', '-o', str(source), str(base)],
                    input=harness, text=True, capture_output=True, timeout=5,
                    env={**environment, variable: password, 'TIMEZONE': timezone})
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                expected = hashlib.sha256((password + '\0' + timezone).encode()).hexdigest()
                self.assertEqual(target.read_text().split()[0], expected)
                self.assertEqual(stat.S_IMODE(target.stat().st_mode), 0o600)
                self.assertFalse(Path(str(target) + '.tmp').exists())
                self.assertEqual(list(cache.glob(target.name + '.tmp.*')), [])
                return target.stat().st_mtime_ns

            password = "spaces ' dollar $ and backslash \\"
            self.assertFalse(cache.exists())
            first = build(password)
            self.assertEqual(calls.read_text(), 'build\n')
            self.assertEqual(build(password), first)
            self.assertEqual(calls.read_text(), 'build\n')
            second = build(password + ' changed')
            self.assertGreater(second, first)
            third = build(password + ' changed', timezone='UTC')
            self.assertGreater(third, second)
            self.assertEqual(build(password + ' changed', timezone='UTC', variable='PASSWD'), third)
            self.assertEqual(calls.read_text(), 'build\nbuild\nbuild\n')


class RootfsSettingsPublicationTest(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.cache = self.root / 'cache'
        self.cache.mkdir()
        self.target = self.cache / 'ubuntu-base-24.04.5-base-koheron-armhf.tgz.settings.sha256'
        password = "settings ' dollar $ and backslash \\"
        fingerprint = hashlib.sha256((password + '\0Europe/Paris').encode()).hexdigest()
        self.target.write_text(fingerprint + '  -\n')
        self.target.chmod(0o600)
        self.original = self.target.read_bytes()
        self.timestamp = self.target.stat().st_mtime_ns
        self.bin = self.root / 'bin'
        self.bin.mkdir()
        self.makefile = self.root / 'Makefile'
        self.makefile.write_text(f'''\
SHELL := /bin/bash
.SHELLFLAGS := -eu -o pipefail -c
.DELETE_ON_ERROR:
TMP := {self.cache}
OS_PATH := {SDK}/os
UBUNTU_ARCH := armhf
UBUNTU_VERSION := 24.04.5
.PHONY: FORCE
FORCE:
include {SDK}/os/rootfs.mk
''')
        self.environment = {
            **os.environ, 'PASSWORD': password, 'TIMEZONE': 'Europe/Paris',
            'PATH': str(self.bin) + ':' + os.environ['PATH'],
        }
        self.argv = ['make', '--no-print-directory', '-f', str(self.makefile), str(self.target)]

    def make(self, **environment):
        return subprocess.run(self.argv, capture_output=True, text=True, timeout=10,
                              env={**self.environment, **environment})

    def assert_previous_settings_preserved(self):
        self.assertEqual(self.target.read_bytes(), self.original)
        self.assertEqual(self.target.stat().st_mtime_ns, self.timestamp)
        self.assertEqual(stat.S_IMODE(self.target.stat().st_mode), 0o600)
        self.assertEqual(list(self.cache.glob(self.target.name + '.tmp*')), [])

    def test_concurrent_unchanged_settings_checks_keep_their_own_temporary_files(self):
        ready = self.root / 'ready'
        release = self.root / 'release'
        comparator = self.bin / 'cmp'
        comparator.write_text(f'''\
#!/usr/bin/python3
import os, sys, time
from pathlib import Path
if os.environ.get('FINGERPRINT_RUN_ID') == 'second':
    Path({str(ready)!r}).touch()
    deadline = time.monotonic() + 5
    while not Path({str(release)!r}).exists():
        if time.monotonic() > deadline:
            sys.exit(99)
        time.sleep(0.01)
os.execv('/usr/bin/cmp', ['cmp', *sys.argv[1:]])
''')
        comparator.chmod(0o755)
        second = subprocess.Popen(self.argv, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                  text=True, env={**self.environment, 'FINGERPRINT_RUN_ID': 'second'})
        try:
            deadline = time.monotonic() + 5
            while not ready.exists():
                if second.poll() is not None or time.monotonic() > deadline:
                    stdout, stderr = second.communicate(timeout=1)
                    self.fail('Second check did not reach comparison: ' + stdout + stderr)
                time.sleep(0.01)
            first = self.make(FINGERPRINT_RUN_ID='first')
            self.assertEqual(first.returncode, 0, first.stdout + first.stderr)
            release.touch()
            stdout, stderr = second.communicate(timeout=5)
            self.assertEqual(second.returncode, 0, stdout + stderr)
        finally:
            release.touch()
            if second.poll() is None:
                second.kill()
            second.communicate(timeout=5)
        self.assert_previous_settings_preserved()

    def failed_hash(self, mode):
        checksum = self.bin / 'sha256sum'
        checksum.write_text('''\
#!/bin/sh
cat >/dev/null
printf 'partial fingerprint\\n'
if [ "$FINGERPRINT_HASH_MODE" = term ]; then
    kill -TERM "$PPID"
else
    exit 19
fi
''')
        checksum.chmod(0o755)
        return self.make(FINGERPRINT_HASH_MODE=mode)

    def test_failed_hash_preserves_settings_and_removes_temporary_file(self):
        result = self.failed_hash('fail')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('19', result.stderr)
        self.assert_previous_settings_preserved()

    def test_interrupted_hash_preserves_settings_and_removes_temporary_file(self):
        result = self.failed_hash('term')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('143', result.stderr)
        self.assert_previous_settings_preserved()


if __name__ == '__main__':
    unittest.main()
