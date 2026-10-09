"""Stage the production watchdog drop-in through Make for both SoC families."""
import configparser
from pathlib import Path
import stat
import subprocess
import tempfile
import unittest

SDK = Path(__file__).resolve().parents[2]


class WatchdogConfigTest(unittest.TestCase):
    def test_platform_selection_and_cached_overlay_refresh(self):
        with tempfile.TemporaryDirectory() as directory:
            overlay = Path(directory) / 'overlay'
            target = overlay / 'etc/systemd/system.conf.d/60-koheron-watchdog.conf'
            harness = f'''\
SHELL := /bin/bash
.SHELLFLAGS := -eu -o pipefail -c
OS_PATH := {SDK}/os
override OVERLAY_DIR := {overlay}
.PHONY: FORCE
FORCE:
include {SDK}/os/rootfs.mk
'''

            def stage(platform):
                result = subprocess.run(
                    ['make', '--no-print-directory', '-f', '-',
                     f'ZYNQ_TYPE={platform}', str(target)], input=harness,
                    text=True, capture_output=True, timeout=5)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertEqual(stat.S_IMODE(target.stat().st_mode), 0o644)
                self.assertFalse(Path(str(target) + '.tmp').exists())
                return target.read_text(), target.stat().st_mtime_ns

            active, first_time = stage('zynq')
            config = configparser.ConfigParser()
            config.read_string(active)
            self.assertEqual(dict(config['Manager']), {'rebootwatchdogsec': '8min'})
            self.assertEqual(stage('zynq'), (active, first_time))
            for platform in ('zynqmp', ''):
                with self.subTest(platform=platform):
                    neutral, timestamp = stage(platform)
                    config = configparser.ConfigParser()
                    config.read_string(neutral)
                    self.assertEqual(config.sections(), [])
                    self.assertEqual(stage(platform), (neutral, timestamp))
            self.assertEqual(stage('zynq')[0], active)

    def test_configuration_is_an_overlay_archive_dependency(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            target = root / 'overlay/etc/systemd/system.conf.d/60-koheron-watchdog.conf'
            harness = f'''\
SHELL := /bin/bash
OS_PATH := {SDK}/os
override OVERLAY_DIR := {root}/overlay
override OVERLAY_TAR := {root}/overlay.tar
.PHONY: FORCE
FORCE:
include {SDK}/os/rootfs.mk
print-dependencies:
\t@printf '%s\\n' $(OVERLAY_FILES)
'''
            result = subprocess.run(['make', '--no-print-directory', '-f', '-',
                                     'ZYNQ_TYPE=zynq', 'print-dependencies'],
                                    input=harness, text=True, capture_output=True, timeout=5)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn(str(target), result.stdout.splitlines())


if __name__ == '__main__':
    unittest.main()
