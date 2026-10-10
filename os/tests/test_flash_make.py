"""Flash must consume an existing image without invoking image assembly."""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


SDK = Path(__file__).resolve().parents[2]


class FlashMakeTest(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        os_path = self.root / 'os'
        (os_path / 'scripts').mkdir(parents=True)
        shutil.copyfile(SDK / 'os/compiler-settings.mk', os_path / 'compiler-settings.mk')
        (os_path / 'scripts/flash_all.py').write_text('''import pathlib, sys
root = pathlib.Path(sys.argv[1]).parent
with (root / 'events').open('a') as stream:
    stream.write('flash ' + pathlib.Path(sys.argv[1]).read_text() + '\\n')
''')
        for name in ('build_image.sh', 'finalize_rootfs.sh', 'chroot_overlay.sh'):
            (os_path / 'scripts' / name).touch()
        (os_path / 'extlinux.conf').touch()
        for name in ('requirements.stamp', 'base.tgz', 'overlay.tar'):
            (self.root / name).touch()
        venv = self.root / 'venv'
        (venv / 'bin').mkdir(parents=True)
        (venv / 'bin/python3').symlink_to(sys.executable)
        self.release = self.root / 'test-review.zip'
        self.harness = self.root / 'Makefile'
        self.harness.write_text(f'''SHELL := /bin/bash
.SHELLFLAGS := -eu -o pipefail -c
OS_PATH := {os_path}
WEB_PATH := {SDK}/web
SERVER_PATH := {SDK}/server
TMP := {self.root}
TMP_PROJECT_PATH := {self.root}
TMP_OS_PATH := {self.root}/os-output
BOARD := test
NAME := review
CFG := example/config.mk
VENV := {venv}
PYTHON_REQUIREMENTS_STAMP := {self.root}/requirements.stamp
.PHONY: FORCE
FORCE:
include {SDK}/os/rootfs.mk
$(RELEASE_ZIP):
\t@sleep 0.05
\t@echo assemble >> {self.root}/events
\t@printf 'updated' > $@
''')

    def make(self, *goals):
        return subprocess.run(['make', '--no-print-directory', '-j4', '-f', str(self.harness),
            f'BASE_ROOTFS_TAR={self.root}/base.tgz', f'OVERLAY_TAR={self.root}/overlay.tar',
            '-o', str(self.root / 'base.tgz'), '-o', str(self.root / 'overlay.tar'),
            '-o', str(self.root / 'manifest-test-review.txt'), *goals],
            cwd=self.root, text=True, capture_output=True, timeout=10,
            env={**os.environ, 'MAKEFLAGS': '', 'MFLAGS': ''})

    def test_flash_preserves_existing_image_even_when_build_time_changes(self):
        self.release.write_text('existing')
        stamp = self.release.stat().st_mtime_ns
        result = self.make('BUILD_TIME_UTC=20990101.000000', 'flash')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(self.release.stat().st_mtime_ns, stamp)
        self.assertEqual((self.root / 'events').read_text(), 'flash existing\n')

    def test_missing_image_fails_before_flashing(self):
        result = self.make('flash')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Run make CFG=example/config.mk image first', result.stderr)
        self.assertFalse((self.root / 'events').exists())

    def test_explicit_image_and_flash_goals_wait_for_assembly(self):
        self.release.write_text('existing')
        source = self.root / 'os/scripts/build_image.sh'
        source.write_text('# changed image input\n')
        # Back-to-back writes can share a filesystem timestamp.
        newer = self.release.stat().st_mtime_ns + 1
        os.utime(source, ns=(newer, newer))
        result = self.make('image', 'flash')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual((self.root / 'events').read_text(), 'assemble\nflash updated\n')

    def test_explicit_unchanged_image_and_flash_reuse_existing_archive(self):
        self.release.write_text('existing')
        result = self.make('image', 'flash')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual((self.root / 'events').read_text(), 'flash existing\n')


if __name__ == '__main__':
    unittest.main()
