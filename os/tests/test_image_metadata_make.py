"""Exercise metadata, overlay and release caching without assembling an SD image."""
import hashlib
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
import zipfile


SDK = Path(__file__).resolve().parents[2]


class ImageMetadataMakeTest(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.overlay = self.root / 'os/rootfs_overlay'
        self.runtime = self.overlay / 'usr/local/share/runtime.txt'
        self.runtime.parent.mkdir(parents=True)
        self.runtime.write_text('first runtime\n')
        self.kernel = self.root / 'kernel.itb'
        self.kernel.write_text('first kernel\n')
        self.source = self.root / 'ubuntu.tgz'
        self.source.write_text('cached source\n')
        self.base = self.root / 'base.tgz'
        self.base.write_text('cached rootfs\n')
        fingerprint = hashlib.sha256(b'changeme\0Europe/Paris').hexdigest()
        Path(str(self.base) + '.settings.sha256').write_text(fingerprint + '  -\n')
        os.utime(str(self.base) + '.settings.sha256', (100, 100))
        self.manifest = self.root / 'manifest-red-pitaya-demo.txt'
        self.release = self.overlay / 'etc/koheron-release'
        self.archive = self.root / 'red-pitaya-demo.zip'
        self.tar = self.root / 'os/rootfs_overlay.tar'
        self.events = self.root / 'events'
        vendor = self.root / 'vendor tools'
        for tool in ('vivado', 'vitis'):
            executable = vendor / 'bin' / tool
            executable.parent.mkdir(parents=True, exist_ok=True)
            executable.write_text(f'''#!/bin/sh
echo query-{tool} >> "{self.events}"
echo '****** {tool.title()} v2026.1 (64-bit)'
echo '  **** SW Build 12345 on 2026-06-16'
''')
            executable.chmod(0o755)
        runner = self.root / 'image.py'
        runner.write_text(f'''import pathlib, zipfile
root = pathlib.Path({str(self.root)!r})
with (root / 'events').open('a') as stream: stream.write('image\\n')
with zipfile.ZipFile(root / 'red-pitaya-demo.zip', 'w') as archive:
    archive.write(root / 'manifest-red-pitaya-demo.txt', 'manifest.txt')
''')
        self.harness = self.root / 'Makefile'
        self.harness.write_text(f'''SHELL := /bin/bash
.SHELLFLAGS := -eu -o pipefail -c
.DELETE_ON_ERROR:
SDK_PATH := {SDK}
OS_PATH := {SDK}/os
TMP := {self.root}
TMP_PROJECT_PATH := {self.root}
TMP_OS_PATH := {self.root}/os
BOARD := red-pitaya
NAME := demo
UBUNTU_ARCH := armhf
GCC_ARCH := arm-linux-gnueabihf
GCC_VERSION := 15
KOHERON_VERSION := 1.0
LINUX_TAG := first-kernel
UBOOT_TAG := first-uboot
DOCKER_ROOT := python3 {runner}
VIVADO_PATH := {vendor}
VITIS_PATH := {vendor}
OS_FILES := {self.kernel}
OVERLAY_FILES := {self.runtime} {self.release} {self.overlay}/usr/local/share/koheron/manifest.txt
.PHONY: FORCE
FORCE:
include {SDK}/os/rootfs.mk
%/:
\tmkdir -p $@
''')

    def make(self, *settings, success=True):
        result = subprocess.run(['make', '--no-print-directory', '-j4', '-f', str(self.harness),
                                 f'BASE_ROOTFS_TAR={self.base}', f'ROOT_TAR_PATH={self.source}',
                                 f'OVERLAY_FILES={self.runtime} {self.release} {self.overlay}/usr/local/share/koheron/manifest.txt',
                                 '-o', str(self.source), *settings, 'image'], cwd=self.root,
                                text=True, capture_output=True, timeout=10,
                                env={k: v for k, v in os.environ.items()
                                     if k not in ('PASSWORD', 'PASSWD', 'TIMEZONE', 'MAKEFLAGS', 'MFLAGS')})
        self.assertEqual(result.returncode == 0, success, result.stdout + result.stderr)
        return result

    def snapshots(self):
        return {p: (hashlib.sha256(p.read_bytes()).hexdigest(), p.stat().st_mtime_ns)
                for p in (self.manifest, self.release, self.tar, self.archive)}

    def assert_cached(self, *settings):
        snapshots = self.snapshots()
        events = self.events.read_text()
        self.make(*settings)
        self.assertEqual(self.snapshots(), snapshots)
        self.assertEqual(self.events.read_text(), events)

    def test_unchanged_image_reuses_metadata_overlay_and_archive(self):
        self.make()
        manifest = dict(line.split('=', 1) for line in self.manifest.read_text().splitlines())
        release = dict(line.split('=', 1) for line in self.release.read_text().splitlines())
        self.assertEqual(manifest['build_id'], release['BUILD_ID'])
        self.assertEqual(manifest['vivado_build'], '12345')
        self.assert_cached()
        for path in (self.runtime, self.kernel, self.base):
            with self.subTest(input=path):
                snapshots = self.snapshots()
                path.write_text(path.read_text() + 'updated\n')
                self.make()
                self.assertNotEqual(self.snapshots(), snapshots)
                self.assert_cached()
        with zipfile.ZipFile(self.archive) as archive:
            self.assertEqual(archive.read('manifest.txt'), self.manifest.read_bytes())

    def test_selection_and_missing_metadata_refresh_matching_files(self):
        self.make()
        self.make('LINUX_TAG=second-kernel')
        self.assertIn('kernel=second-kernel\n', self.manifest.read_text())
        self.assert_cached('LINUX_TAG=second-kernel')
        older = self.root / 'cached-kernel.itb'
        older.write_text('older cached kernel\n')
        os.utime(older, (100, 100))
        selection = f'OS_FILES={older}'
        previous = self.snapshots()
        self.make(selection)
        self.assertNotEqual(self.snapshots(), previous)
        self.assert_cached(selection)
        self.release.unlink()
        self.make(selection)
        self.assertTrue(self.release.is_file())
        self.assert_cached(selection)

    def test_overrides_are_stable_and_failed_generation_preserves_artifacts(self):
        settings = ('BUILD_ID=review-build', 'BUILD_TIME_UTC=20261010.120000')
        self.make(*settings)
        self.assertIn('build_id=review-build\n', self.manifest.read_text())
        self.assertIn('generated_utc=2026-10-10T12:00:00Z\n', self.manifest.read_text())
        self.assert_cached(*settings)
        previous = self.snapshots()
        self.make('BUILD_TIME_UTC=invalid', success=False)
        self.assertEqual(self.snapshots(), previous)
        self.assertEqual(list(self.root.rglob('*.tmp.*')), [])
        self.make(*settings)
        self.assert_cached(*settings)


if __name__ == '__main__':
    unittest.main()
