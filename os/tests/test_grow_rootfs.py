"""Run the first-boot helper with mocked disk commands; never touch real disks."""
import configparser
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest


OS_PATH = Path(__file__).resolve().parents[1]
SCRIPT = OS_PATH / 'scripts/grow-rootfs-once'


class GrowRootfsTest(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.stamp = self.root / 'state/done'
        self.log = self.root / 'commands'
        self.bin = self.root / 'bin'
        self.bin.mkdir()
        self.script = self.root / 'grow-rootfs-once'
        original = SCRIPT.read_text()
        self.default_stamp = re.search(r'^STAMP=(.+)$', original, re.M).group(1)
        self.script.write_text(original.replace('STAMP=' + self.default_stamp,
                                               'STAMP=' + str(self.stamp), 1))
        mock = self.bin / 'mock'
        mock.write_text('''\
#!/bin/sh
command=${0##*/}
printf '%s %s\\n' "$command" "$*" >> "$COMMAND_LOG"
case "$command" in
  findmnt) printf '%s\\n' "$ROOT_SOURCE" ;;
  growpart) exit "${GROWPART_RESULT:-0}" ;;
  partx) exit "${PARTX_RESULT:-0}" ;;
  udevadm) exit "${UDEV_RESULT:-0}" ;;
  resize2fs) exit "${RESIZE_RESULT:-0}" ;;
esac
''')
        mock.chmod(0o755)
        for name in ('findmnt', 'growpart', 'partx', 'udevadm', 'resize2fs'):
            (self.bin / name).symlink_to(mock)
        self.environment = {**os.environ, 'PATH': str(self.bin) + ':' + os.environ['PATH'],
                            'COMMAND_LOG': str(self.log), 'ROOT_SOURCE': '/dev/mmcblk0p2'}

    def run_helper(self, **overrides):
        return subprocess.run(['/bin/sh', str(self.script)], capture_output=True,
                              text=True, env={**self.environment, **overrides}, timeout=5)

    def commands(self):
        return self.log.read_text().splitlines() if self.log.exists() else []

    def assert_completed(self, result):
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertTrue(self.stamp.exists())
        self.assertIn('resize2fs /dev/mmcblk0p2', self.commands())

    def test_script_and_service_share_one_marker(self):
        unit = configparser.ConfigParser(interpolation=None)
        unit.read(OS_PATH / 'systemd/grow-rootfs-once.service')
        self.assertEqual(unit['Unit']['ConditionPathExists'], '!' + self.default_stamp)
        self.assertFalse(unit.has_option('Service', 'ExecStartPost'))

    def test_success_marks_complete_and_subsequent_run_does_nothing(self):
        self.assert_completed(self.run_helper())
        commands = self.commands()
        self.assertEqual(self.run_helper().returncode, 0)
        self.assertEqual(self.commands(), commands)

    def test_nochange_still_refreshes_partition_and_grows_filesystem(self):
        self.assert_completed(self.run_helper(GROWPART_RESULT='1'))
        self.assertIn('partx --update --nr 2 /dev/mmcblk0', self.commands())

    def test_partition_failure_leaves_retry_available(self):
        result = self.run_helper(GROWPART_RESULT='2')
        self.assertEqual(result.returncode, 2)
        self.assertFalse(self.stamp.exists())
        self.assertFalse(any(c.startswith(('partx ', 'resize2fs ')) for c in self.commands()))
        self.assert_completed(self.run_helper())

    def test_missing_growpart_is_not_treated_as_nochange(self):
        result = self.run_helper(GROWPART_RESULT='127')
        self.assertEqual(result.returncode, 127)
        self.assertFalse(self.stamp.exists())

    def test_kernel_partition_refresh_failure_can_retry_after_nochange(self):
        result = self.run_helper(PARTX_RESULT='1')
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse(self.stamp.exists())
        self.assertNotIn('resize2fs /dev/mmcblk0p2', self.commands())
        self.assert_completed(self.run_helper(GROWPART_RESULT='1'))

    def test_udev_failure_is_not_marked_complete(self):
        self.assertNotEqual(self.run_helper(UDEV_RESULT='1').returncode, 0)
        self.assertFalse(self.stamp.exists())
        self.assertNotIn('resize2fs /dev/mmcblk0p2', self.commands())

    def test_filesystem_failure_can_retry_after_partition_already_grew(self):
        self.assertNotEqual(self.run_helper(RESIZE_RESULT='1').returncode, 0)
        self.assertFalse(self.stamp.exists())
        self.assert_completed(self.run_helper(GROWPART_RESULT='1'))

    def test_unsupported_root_is_skipped_without_completion_marker(self):
        result = self.run_helper(ROOT_SOURCE='/dev/sda2')
        self.assertEqual(result.returncode, 0)
        self.assertFalse(self.stamp.exists())
        self.assertEqual(self.commands(), ['findmnt -no SOURCE /'])


if __name__ == '__main__':
    unittest.main()
