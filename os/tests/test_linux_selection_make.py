"""Switch populated kernel caches without recompiling or downloading anything."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest


SDK = Path(__file__).resolve().parents[2]


class LinuxSelectionMakeTest(unittest.TestCase):
    def test_cached_release_switches_refresh_kernel_and_board_overlay(self):
        for arch, image in (('arm', 'zImage'), ('arm64', 'Image')):
            with self.subTest(arch=arch), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                for version in ('old', 'new'):
                    kernel = root / f'linux-{version}'
                    output = kernel / f'arch/{arch}/boot/{image}'
                    output.parent.mkdir(parents=True)
                    output.write_text(version)
                    for name in ('.built_all', '.unpacked'):
                        (kernel / name).touch()
                        os.utime(kernel / name, (100, 100))
                    overlay = kernel / 'board.dtso'
                    overlay.write_text(version)
                    os.utime(overlay, (100, 100))
                harness = f'''SHELL := /bin/bash
.SHELLFLAGS := -eu -o pipefail -c
OS_PATH := {SDK}/os
TMP_OS_PATH := {root}/os
TMP := {root}
ARCH := {arch}
KERNEL_BIN := {image}
LINUX_TAG = $(LINUX_VERSION)
LINUX_PATH = {root}/linux-$(LINUX_VERSION)
LINUX_BUILD_STAMP = $(LINUX_PATH)/.built_all
BOARD_DTSO = $(LINUX_PATH)/board.dtso
%/:
\tmkdir -p $@
include {SDK}/os/os.mk
'''
                target = root / f'os/{image}'
                staged_overlay = root / 'os/board-overlay/board.dtso'
                for version in ('old', 'new', 'old'):
                    result = subprocess.run(['make', '-j4', '--no-print-directory', '-f', '-',
                        f'LINUX_VERSION={version}', str(target), str(staged_overlay)],
                        input=harness, text=True, capture_output=True, timeout=10)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    self.assertEqual(target.read_text(), version)
                    self.assertEqual(staged_overlay.read_text(), version)
                times = [p.stat().st_mtime_ns for p in (target, staged_overlay)]
                result = subprocess.run(['make', '--no-print-directory', '-f', '-',
                    'LINUX_VERSION=old', str(target), str(staged_overlay)],
                    input=harness, text=True, capture_output=True, timeout=10)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertEqual(times, [p.stat().st_mtime_ns for p in (target, staged_overlay)])


if __name__ == '__main__':
    unittest.main()
