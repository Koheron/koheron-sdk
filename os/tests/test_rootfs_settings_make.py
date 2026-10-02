"""Check cache invalidation for rootfs settings without downloading or building."""
import hashlib
import os
from pathlib import Path
import stat
import subprocess
import tempfile
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


if __name__ == '__main__':
    unittest.main()
