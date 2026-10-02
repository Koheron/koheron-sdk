"""Stage board overlays from a fresh kernel tree without downloading sources."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest


SDK = Path(__file__).resolve().parents[2]


class BoardOverlayMakeTest(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.kernel = self.root / 'kernel'
        self.source = self.kernel / 'arch/arm64/boot/dts/xilinx/board.dtso'
        self.archive = self.root / 'archive'
        self.archive.write_text('first overlay\n')
        self.target = self.root / 'os/board-overlay/board.dtso'

    def build(self, source=None, omit_overlay=False):
        harness = f'''\
SHELL := /bin/bash
.SHELLFLAGS := -eu -o pipefail -c
.DELETE_ON_ERROR:
OS_PATH := {SDK}/os
TMP_OS_PATH := {self.root}/os
LINUX_PATH := {self.kernel}
BOARD_DTSO := {source or self.source}
%/:
\tmkdir -p $@
$(LINUX_PATH)/.unpacked: {self.archive}
\tmkdir -p {self.source.parent}
\t{'true' if omit_overlay else f'cp -p {self.archive} {self.source}'}
\ttouch $@
\techo unpack >> {self.root}/events
include {SDK}/os/os.mk
'''
        return subprocess.run(['make', '--no-print-directory', '-f', '-',
                               str(self.target)], input=harness, capture_output=True,
                              text=True, timeout=5)

    def assert_success(self, result):
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_fresh_kernel_unpacks_before_staging_and_restages_after_unpack(self):
        self.assert_success(self.build())
        self.assertEqual(self.target.read_text(), 'first overlay\n')
        timestamp = self.target.stat().st_mtime_ns
        self.assert_success(self.build())
        self.assertEqual(self.target.stat().st_mtime_ns, timestamp)
        self.assertEqual((self.root / 'events').read_text(), 'unpack\n')

        self.archive.write_text('second overlay\n')
        # A new extraction can preserve a source's old archive timestamp.
        os.utime(self.archive, (100, 100))
        os.utime(self.kernel / '.unpacked', (1, 1))
        self.assert_success(self.build())
        self.assertEqual(self.target.read_text(), 'second overlay\n')
        self.assertEqual((self.root / 'events').read_text(), 'unpack\nunpack\n')

    def test_checked_in_overlay_does_not_require_kernel_unpack(self):
        source = self.root / 'board.dtso'
        source.write_text('board-local overlay\n')
        self.assert_success(self.build(source=source))
        self.assertEqual(self.target.read_text(), 'board-local overlay\n')
        self.assertFalse((self.kernel / '.unpacked').exists())

    def test_missing_overlay_after_unpack_fails_without_staging(self):
        result = self.build(omit_overlay=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Missing board overlay after kernel unpack', result.stderr)
        self.assertFalse(self.target.exists())


if __name__ == '__main__':
    unittest.main()
