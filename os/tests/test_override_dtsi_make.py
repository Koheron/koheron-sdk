"""Switching the selected overlay source must restage the copied DTSI."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest


SDK = Path(__file__).resolve().parents[2]


class OverrideDtsiMakeTest(unittest.TestCase):
    def test_switching_to_older_source_restages_override(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            first = root / 'first.dtsi'
            second = root / 'second.dtsi'
            first.write_text('first overlay\n')
            second.write_text('second overlay\n')
            os.utime(second, (100, 100))
            output = root / 'os/pl-overlay/override.dtsi'
            harness = f'''SHELL := /bin/bash
.SHELLFLAGS := -eu -o pipefail -c
.DELETE_ON_ERROR:
OS_PATH := {SDK}/os
TMP_OS_PATH := {root}/os
%/:
\tmkdir -p $@
include {SDK}/os/os.mk
'''

            def build(source):
                result = subprocess.run(
                    ['make', '--no-print-directory', '-j4', '-f', '-', str(output),
                     f'OVERRIDE_DTSI={source}'], input=harness, text=True,
                    capture_output=True, timeout=10)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

            build(first)
            self.assertEqual(output.read_text(), 'first overlay\n')
            initial_mtime = output.stat().st_mtime_ns
            build(first)
            self.assertEqual(output.stat().st_mtime_ns, initial_mtime)
            build(second)
            self.assertEqual(output.read_text(), 'second overlay\n')
            build(first)
            self.assertEqual(output.read_text(), 'first overlay\n')


if __name__ == '__main__':
    unittest.main()
