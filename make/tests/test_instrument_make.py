"""Exercise actual instrument packaging with cached build outputs."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
import zipfile


SDK = Path(__file__).resolve().parents[2]


class InstrumentMakeTest(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.project = self.root / 'project'
        self.project.mkdir()
        for name in ('demo.bit', 'demo.bit.bin', 'serverd', 'drivers.json', 'pl.dtbo',
                     'version', 'instrument.json', 'keep.css', 'obsolete.css'):
            (self.project / name).write_text(name + '\n')
        (self.project / 'version').write_text('0.3.0\n')
        (self.project / 'instrument.json').write_text(json.dumps({
            'format': 1, 'board': 'red-pitaya', 'architecture': 'armhf',
            'sdk_version': '1.0', 'sdk_commit': 'unknown',
            'min_runtime_api': 1}) + '\n')
        python = self.root / 'venv/bin/python3'
        python.parent.mkdir(parents=True)
        python.symlink_to(sys.executable)
        self.archive = self.project / 'demo.zip'
        self.board_copy = self.root / 'cache/red-pitaya/instruments/demo.zip'
        self.harness = self.root / 'Makefile'
        self.harness.write_text(f'''SHELL := /bin/bash
.SHELLFLAGS := -eu -o pipefail -c
.DELETE_ON_ERROR:
SDK_PATH := {SDK}
TMP := {self.root}/cache
TMP_PROJECT_PATH := {self.project}
NAME := demo
BOARD := red-pitaya
VERSION := 0.3.0
UBUNTU_ARCH := armhf
KOHERON_VERSION := 1.0
VENV := {self.root}/venv
INSTRUMENT_ZIP := $(TMP_PROJECT_PATH)/demo.zip
SERVER := $(TMP_PROJECT_PATH)/serverd
DRIVERS_JSON_OUT := $(TMP_PROJECT_PATH)/drivers.json
BITSTREAM := $(TMP_PROJECT_PATH)/demo.bit
VERSION_FILE := $(TMP_PROJECT_PATH)/version
WEB_ASSETS := $(TMP_PROJECT_PATH)/keep.css $(TMP_PROJECT_PATH)/obsolete.css
include {SDK}/make/instrument.mk
%/:
\tmkdir -p $@
''')
        self.bin = self.root / 'bin'
        self.bin.mkdir()

    def make(self, *settings, success=True):
        result = subprocess.run(['make', '--no-print-directory', '-j4', '-f', str(self.harness),
                                 *settings, str(self.archive), str(self.board_copy)],
                                cwd=self.root, text=True, capture_output=True, timeout=10,
                                env={**os.environ, 'PATH': str(self.bin) + ':' + os.environ['PATH'],
                                     'MAKEFLAGS': '', 'MFLAGS': ''})
        self.assertEqual(result.returncode == 0, success, result.stdout + result.stderr)
        return result

    def snapshots(self):
        return {p: (hashlib.sha256(p.read_bytes()).hexdigest(), p.stat().st_mtime_ns)
                for p in (self.archive, self.board_copy)}

    def test_removed_assets_leave_archive_and_unchanged_build_is_cached(self):
        self.make()
        original = self.snapshots()
        self.make()
        self.assertEqual(self.snapshots(), original)
        (self.project / 'obsolete.css').unlink()
        selection = f'WEB_ASSETS={self.project}/keep.css'
        self.make(selection)
        with zipfile.ZipFile(self.archive) as archive:
            self.assertIn('keep.css', archive.namelist())
            self.assertNotIn('obsolete.css', archive.namelist())
        self.assertEqual(self.archive.read_bytes(), self.board_copy.read_bytes())
        updated = self.snapshots()
        self.make(selection)
        self.assertEqual(self.snapshots(), updated)

    def test_missing_board_copy_is_restored_without_repackaging(self):
        self.make()
        original = self.snapshots()[self.archive]
        self.board_copy.unlink()
        self.make()
        self.assertEqual(self.snapshots()[self.archive], original)
        self.assertEqual(self.archive.read_bytes(), self.board_copy.read_bytes())

    def test_board_copy_follows_selected_cached_project_with_same_name(self):
        self.make()
        original = self.snapshots()[self.archive]
        other = self.root / 'other-project'
        shutil.copytree(self.project, other)
        (other / 'keep.css').write_text('other project stylesheet\n')
        self.make(f'TMP_PROJECT_PATH={other}')
        self.assertEqual(self.board_copy.read_bytes(), (other / 'demo.zip').read_bytes())
        self.assertNotEqual(self.archive.read_bytes(), self.board_copy.read_bytes())
        self.make()
        self.assertEqual(self.snapshots()[self.archive], original)
        self.assertEqual(self.archive.read_bytes(), self.board_copy.read_bytes())
        updated = self.snapshots()
        self.make()
        self.assertEqual(self.snapshots(), updated)

    def test_version_override_refreshes_archive_and_then_is_cached(self):
        self.make()
        self.make('VERSION=0.4.0')
        with zipfile.ZipFile(self.archive) as archive:
            self.assertEqual(archive.read('version'), b'0.4.0\n')
        updated = self.snapshots()
        self.make('VERSION=0.4.0')
        self.assertEqual(self.snapshots(), updated)

    def test_compatibility_overrides_refresh_manifest_and_then_are_cached(self):
        self.make()
        for setting, field, value in (('KOHERON_VERSION=2.0', 'sdk_version', '2.0'),
                                      ('UBUNTU_ARCH=arm64', 'architecture', 'arm64'),
                                      ('BOARD=alpha250', 'board', 'alpha250')):
            with self.subTest(setting=setting):
                self.make(setting)
                with zipfile.ZipFile(self.archive) as archive:
                    self.assertEqual(json.loads(archive.read('instrument.json'))[field], value)
                updated = self.snapshots()
                self.make(setting)
                self.assertEqual(self.snapshots(), updated)

    def test_commit_change_refreshes_package_without_touching_server(self):
        self.make('INSTRUMENT_SDK_COMMIT=aaaaaaaaaaaa')
        server_time = (self.project / 'serverd').stat().st_mtime_ns
        original = self.snapshots()
        self.make('INSTRUMENT_SDK_COMMIT=bbbbbbbbbbbb')
        with zipfile.ZipFile(self.archive) as archive:
            self.assertEqual(json.loads(archive.read('instrument.json'))['sdk_commit'],
                             'bbbbbbbbbbbb')
        self.assertEqual((self.project / 'serverd').stat().st_mtime_ns, server_time)
        self.assertNotEqual(self.snapshots(), original)
        updated = self.snapshots()
        self.make('INSTRUMENT_SDK_COMMIT=bbbbbbbbbbbb')
        self.assertEqual(self.snapshots(), updated)

    def test_failed_and_interrupted_zip_preserve_previous_archives(self):
        self.make()
        original = self.snapshots()
        zipper = self.bin / 'zip'
        for failure in ('exit 19', 'kill -TERM "$PPID"; exit 0'):
            with self.subTest(failure=failure):
                zipper.write_text('#!/bin/sh\nprintf partial > "$2"\n' + failure + '\n')
                zipper.chmod(0o755)
                self.make('-W', str(self.project / 'serverd'), success=False)
                self.assertEqual(self.snapshots(), original)
                self.assertEqual(list(self.project.glob('demo.zip.tmp.*')), [])
        zipper.unlink()
        self.make('-W', str(self.project / 'serverd'))

    def test_failed_board_copy_preserves_previous_copy_and_can_retry(self):
        self.make()
        original = self.snapshots()[self.board_copy]
        (self.project / 'keep.css').write_text('updated stylesheet\n')
        copier = self.bin / 'cp'
        copier.write_text('#!/bin/sh\nprintf partial > "$3"\nexit 17\n')
        copier.chmod(0o755)
        self.make(success=False)
        self.assertEqual(self.snapshots()[self.board_copy], original)
        self.assertEqual(list(self.board_copy.parent.glob('demo.zip.tmp.*')), [])
        copier.unlink()
        self.make()
        self.assertEqual(self.archive.read_bytes(), self.board_copy.read_bytes())

    def test_interruption_after_publication_keeps_complete_archive_for_retry(self):
        self.make()
        (self.project / 'keep.css').write_text('updated stylesheet\n')
        mover = self.bin / 'mv'
        mover.write_text(f'''#!{sys.executable}
import os, signal, subprocess, sys
status = subprocess.call(['/usr/bin/mv', *sys.argv[1:]])
if status == 0 and sys.argv[-1] == {str(self.archive)!r}:
    os.kill(os.getppid(), signal.SIGTERM)
raise SystemExit(status)
''')
        mover.chmod(0o755)
        self.make(success=False)
        with zipfile.ZipFile(self.archive) as archive:
            self.assertEqual(archive.read('keep.css'), b'updated stylesheet\n')
        mover.unlink()
        self.make()
        self.assertEqual(self.archive.read_bytes(), self.board_copy.read_bytes())


if __name__ == '__main__':
    unittest.main()
