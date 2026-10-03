"""Exercise rootfs download caching with real checksum tools and a fake curl."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest


SDK = Path(__file__).resolve().parents[2]


class RootfsDownloadMakeTest(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.cache = self.root / 'cache'
        self.fixtures = self.root / 'fixtures'
        self.fixtures.mkdir()
        self.checksums = self.cache / 'ubuntu-base-24.04.5-SHA256SUMS'
        self.payloads = {}
        lines = []
        for arch in ('armhf', 'arm64'):
            name = self.archive(arch).name
            payload = f'Ubuntu source for {arch}\n'.encode()
            self.payloads[arch] = payload
            (self.fixtures / name).write_bytes(payload)
            lines.append(f'{hashlib.sha256(payload).hexdigest()} *{name}\n')
        (self.fixtures / 'SHA256SUMS').write_text(''.join(lines))
        self.events = self.root / 'downloads'
        self.bin = self.root / 'bin'
        self.bin.mkdir()
        curl = self.bin / 'curl'
        curl.write_text('''\
#!/usr/bin/python3
import json, os, signal, sys
from pathlib import Path
args = sys.argv[1:]
output = args.index('-o')
name = args[output - 1].rsplit('/', 1)[-1]
destination = Path(args[output + 1])
with Path(os.environ['DOWNLOAD_LOG']).open('a') as log:
    log.write(json.dumps(args) + '\\n')
kind = 'checksum' if name == 'SHA256SUMS' else 'archive'
if os.environ.get('CURL_FAIL') in ('all', kind):
    destination.write_bytes(b'partial download')
    sys.exit(22)
if os.environ.get('CURL_TERM') == kind:
    destination.write_bytes(b'partial download')
    os.kill(os.getppid(), signal.SIGTERM)
    sys.exit(0)
destination.write_bytes((Path(os.environ['DOWNLOAD_FIXTURES']) / name).read_bytes())
''')
        curl.chmod(0o755)
        self.builds = self.root / 'base-builds'
        self.runner = self.root / 'docker.py'
        self.runner.write_text(f'''\
from pathlib import Path
import sys
Path(sys.argv[4]).write_bytes(b'configured rootfs')
with Path({str(self.builds)!r}).open('a') as log:
    log.write('build\\n')
''')
        self.environment = {
            **{k: v for k, v in os.environ.items() if k not in ('PASSWORD', 'PASSWD', 'TIMEZONE')},
            'PATH': str(self.bin) + ':' + os.environ['PATH'],
            'DOWNLOAD_LOG': str(self.events), 'DOWNLOAD_FIXTURES': str(self.fixtures),
        }

    def archive(self, arch='armhf'):
        return self.cache / f'ubuntu-base-24.04.5-base-{arch}.tar.gz'

    def make(self, arch='armhf', target=None, force=False, **environment):
        harness = f'''\
SHELL := /bin/bash
.SHELLFLAGS := -eu -o pipefail -c
.DELETE_ON_ERROR:
TMP := {self.cache}
OS_PATH := {SDK}/os
UBUNTU_VERSION := 24.04.5
UBUNTU_ARCH := {arch}
DOCKER_ROOT := python3 {self.runner}
ok =
.PHONY: FORCE
FORCE:
include {SDK}/os/rootfs.mk
'''
        argv = ['make', '--no-print-directory', '-f', '-']
        if force:
            argv.append('-B')
        argv.append(str(target or self.archive(arch)))
        return subprocess.run(argv, input=harness, capture_output=True, text=True,
                              timeout=10, env={**self.environment, **environment})

    def assert_success(self, result):
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def downloads(self):
        return [json.loads(line) for line in self.events.read_text().splitlines()]

    def assert_no_partials(self):
        self.assertEqual(list(self.cache.glob('*.download.*')), [])

    def test_fresh_download_supports_both_architectures_and_binary_checksums(self):
        for arch in ('armhf', 'arm64'):
            with self.subTest(arch=arch):
                self.assert_success(self.make(arch))
                self.assertEqual(self.archive(arch).read_bytes(), self.payloads[arch])
        self.assertEqual(len(self.downloads()), 3)
        self.assertEqual(self.checksums.stat().st_mode & 0o777, 0o644)
        self.assert_no_partials()

    def test_cached_source_is_verified_offline_without_rebuilding_base(self):
        base = self.cache / 'ubuntu-base-24.04.5-base-koheron-armhf.tgz'
        self.assert_success(self.make(target=base))
        paths = (self.archive(), self.checksums, base)
        timestamps = [path.stat().st_mtime_ns for path in paths]
        self.assert_success(self.make(target=base, CURL_FAIL='all'))
        self.assertEqual([path.stat().st_mtime_ns for path in paths], timestamps)
        self.assertEqual(len(self.downloads()), 2)
        self.assertEqual(self.builds.read_text(), 'build\n')

    def test_corrupt_cached_source_is_replaced_even_when_its_timestamp_is_newer(self):
        self.assert_success(self.make())
        self.archive().write_bytes(b'corrupt cached archive')
        os.utime(self.archive(), (4102444800, 4102444800))
        self.assert_success(self.make())
        self.assertEqual(self.archive().read_bytes(), self.payloads['armhf'])
        self.assertEqual(len(self.downloads()), 3)
        self.assert_no_partials()

    def test_checksum_mismatch_preserves_existing_source(self):
        self.assert_success(self.make())
        (self.fixtures / self.archive().name).write_bytes(b'wrong download')
        self.archive().write_bytes(b'previous source')
        timestamp = self.archive().stat().st_mtime_ns
        result = self.make()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('SHA-256 mismatch', result.stderr)
        self.assertEqual(self.archive().read_bytes(), b'previous source')
        self.assertEqual(self.archive().stat().st_mtime_ns, timestamp)
        self.assert_no_partials()

    def test_partial_archive_download_does_not_publish_or_destroy_existing_source(self):
        for existing in (False, True):
            with self.subTest(existing=existing):
                if existing:
                    self.archive().write_bytes(b'previous source')
                result = self.make(CURL_FAIL='archive')
                self.assertNotEqual(result.returncode, 0)
                if existing:
                    self.assertEqual(self.archive().read_bytes(), b'previous source')
                else:
                    self.assertFalse(self.archive().exists())
                self.assert_no_partials()

    def test_partial_checksum_download_preserves_previous_list(self):
        self.assert_success(self.make())
        previous = self.checksums.read_bytes()
        timestamp = self.checksums.stat().st_mtime_ns
        result = self.make(target=self.checksums, force=True, CURL_FAIL='checksum')
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.checksums.read_bytes(), previous)
        self.assertEqual(self.checksums.stat().st_mtime_ns, timestamp)
        self.assert_no_partials()

    def test_invalid_checksum_lists_do_not_replace_previous_list(self):
        self.assert_success(self.make())
        previous = self.checksums.read_bytes()
        matching = previous.decode().splitlines()[0] + '\n'
        bad_lists = ('<html>server error</html>\n', matching * 2,
                     f'not-a-hash *{self.archive().name}\n', matching.rstrip() + ' extra\n')
        for contents in bad_lists:
            with self.subTest(contents=contents):
                (self.fixtures / 'SHA256SUMS').write_text(contents)
                result = self.make(target=self.checksums, force=True)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn('No unique valid SHA-256 checksum', result.stderr)
                self.assertEqual(self.checksums.read_bytes(), previous)
                self.assert_no_partials()

    def test_text_checksum_marker_is_supported(self):
        checksum = self.fixtures / 'SHA256SUMS'
        checksum.write_text(checksum.read_text().replace(' *', '  '))
        self.assert_success(self.make())
        self.assertEqual(self.archive().read_bytes(), self.payloads['armhf'])

    def test_interrupted_downloads_remove_partials(self):
        for kind in ('checksum', 'archive'):
            with self.subTest(kind=kind):
                result = self.make(CURL_TERM=kind)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn('143', result.stderr)
                self.assertFalse(self.archive().exists())
                self.assert_no_partials()


if __name__ == '__main__':
    unittest.main()
