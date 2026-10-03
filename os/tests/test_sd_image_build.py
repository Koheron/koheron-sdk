"""Run the image builder with fake disk tools; never attach or mount real disks."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tarfile
import tempfile
import unittest
import zipfile


OS_PATH = Path(__file__).resolve().parents[1]
BUILD = OS_PATH / 'scripts/build_image.sh'


class SDImageBuildTest(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.project = self.root / 'project'
        self.project.mkdir()
        self.artifacts = self.root / 'artifacts'
        self.artifacts.mkdir()
        for name in ('boot.bin', 'kernel.itb'):
            (self.artifacts / name).write_bytes(b'boot artifact')
        (self.project / 'manifest-test.txt').write_text('test manifest\n')
        source = self.root / 'source'
        for name in ('etc/ssh', 'usr/bin', 'proc', 'sys', 'dev', 'run'):
            (source / name).mkdir(parents=True, exist_ok=True)
        self.base = self.root / 'base.tgz'
        with tarfile.open(self.base, 'w:gz') as archive:
            archive.add(source, arcname='.')
        self.overlay = self.root / 'overlay.tar'
        with tarfile.open(self.overlay, 'w'):
            pass
        self.qemu = self.root / 'qemu-arm-static'
        self.qemu.write_text('unused helper\n')
        self.bin = self.root / 'bin'
        self.bin.mkdir()
        self.mounts = self.root / 'mounts'
        self.mounts.mkdir()
        self.events = self.root / 'events'
        mock = self.bin / 'mock'
        mock.write_text('''\
#!/usr/bin/python3
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile

command = Path(sys.argv[0]).name
args = sys.argv[1:]
state_file = Path(os.environ['MOCK_STATE'])
state = json.loads(state_file.read_text()) if state_file.exists() else {'mounted': [], 'end': 2097151}
with open(os.environ['MOCK_EVENTS'], 'a') as log:
    log.write(json.dumps([command, *args]) + '\\n')
result = 0
if command == 'dd':
    destination = next(a[3:] for a in args if a.startswith('of='))
    Path(destination).write_bytes(b'untruncated image')
elif command == 'losetup':
    if '--find' in args:
        print('/dev/mockloop')
    elif '-d' in args:
        assert not state['mounted'], state['mounted']
        result = int(os.environ.get('DETACH_RESULT', '0'))
elif command == 'mktemp':
    template = Path(args[-1])
    packaging = template.name.startswith('.package.')
    print(tempfile.mkdtemp(prefix='.package.' if packaging else template.name.split('.')[0] + '.',
                           dir=template.parent if packaging else os.environ['MOCK_MOUNTS']))
elif command == 'lsblk':
    print('mockloop\\nmockloopp1\\nmockloopp2')
elif command == 'mount':
    if '--make-rslave' not in args:
        state['mounted'].append(args[-1])
        if args[-1].endswith('/sys'):
            result = int(os.environ.get('SYS_MOUNT_RESULT', '0'))
elif command == 'mountpoint':
    result = 0 if args[-1] in state['mounted'] else 1
elif command == 'umount':
    target = args[-1]
    if (os.environ.get('FAIL_UNMOUNT') and target.endswith('/dev')) or any(
            p.startswith(target + '/') for p in state['mounted']):
        result = 17
    elif target in state['mounted']:
        state['mounted'].remove(target)
elif command == 'chroot':
    result = int(os.environ.get('CHROOT_RESULT', '0'))
    if os.environ.get('SEND_TERM'):
        os.kill(os.getppid(), signal.SIGTERM)
elif command == 'blkid':
    print('test-partuuid')
elif command == 'envsubst':
    text = sys.stdin.read()
    for name in ('ROOTUUID', 'BOOTUUID', 'RELEASE_NAME'):
        text = text.replace('${' + name + '}', os.environ[name])
    print(text, end='')
elif command == 'e2fsck':
    result = int(os.environ.get('FSCK_RESULT', '0'))
elif command == 'tune2fs' and '-l' in args:
    print('Block count: 1024\\nBlock size: 4096')
elif command == 'parted' and 'print' in args:
    print('BYT;\\n/dev/mockloop:2097152s:loop:512:512:msdos:;')
    if not (os.environ.get('MISSING_PARTITION') and state.get('resized')):
        print(f"2:32768s:{state['end']}s:100s:ext4::;")
elif command == 'sfdisk':
    sectors = int(sys.stdin.read().strip().split(',')[1])
    result = int(os.environ.get('RESIZE_RESULT', '0'))
    state['resized'] = True
    if result == 0 and not os.environ.get('RESIZE_NOOP'):
        state['end'] = 32768 + sectors - 1
elif command == 'truncate':
    with open(args[-1], 'r+b') as output:
        output.truncate(int(args[1]))
elif command == 'zip':
    Path(args[2]).write_bytes(b'packaged image')
    result = int(os.environ.get('ZIP_RESULT', '0'))
    if result or os.environ.get('ZIP_SEND_TERM'):
        Path(args[2]).with_name('.zip-scratch').write_bytes(b'partial output')
    if os.environ.get('ZIP_SEND_TERM'):
        os.kill(os.getppid(), signal.SIGTERM)
elif command == 'mv':
    result = int(os.environ.get('PUBLISH_RESULT', '0'))
    if result == 0:
        result = subprocess.run(['/bin/mv', *args]).returncode
elif command not in ('parted', 'partprobe', 'udevadm', 'sleep', 'mkfs.vfat',
                     'mkfs.ext4', 'tune2fs', 'chown', 'resize2fs', 'blockdev'):
    raise RuntimeError(command)
state_file.write_text(json.dumps(state))
sys.exit(result)
''')
        mock.chmod(0o755)
        for name in ('dd', 'losetup', 'mktemp', 'lsblk', 'mount', 'mountpoint',
                     'umount', 'chroot', 'blkid', 'envsubst', 'e2fsck', 'tune2fs',
                     'parted', 'sfdisk', 'truncate', 'zip', 'partprobe', 'udevadm',
                     'sleep', 'mkfs.vfat', 'mkfs.ext4', 'chown', 'resize2fs', 'blockdev', 'mv'):
            (self.bin / name).symlink_to(mock)
        self.environment = {
            **os.environ, 'PATH': str(self.bin) + ':' + os.environ['PATH'],
            'BASE_ROOTFS_TAR': str(self.base), 'EXTLINUX_CONF': str(OS_PATH / 'extlinux.conf'),
            'MOCK_STATE': str(self.root / 'state'), 'MOCK_EVENTS': str(self.events),
            'MOCK_MOUNTS': str(self.mounts),
        }

    def build(self, **overrides):
        # Only the fake device's block-node predicate is overridden. Every
        # real disk operation is replaced by a mock executable above.
        harness = '''\
function [ {
  if builtin [ "$1" = -b ]; then
    case "$2" in /dev/mockloopp1|/dev/mockloopp2) return 0;; esac
  fi
  builtin [ "$@"
}
source "$@"
'''
        return subprocess.run(
            ['bash', '-c', harness, 'image-test', str(BUILD), str(self.project),
             str(OS_PATH), str(self.artifacts), 'unused', str(self.overlay),
             str(self.qemu), 'test'], capture_output=True, text=True, timeout=15,
            env={**self.environment, **overrides})

    def commands(self):
        return [json.loads(line) for line in self.events.read_text().splitlines()]

    def assert_failed_before_packaging(self, result, code):
        self.assertEqual(result.returncode, code, result.stdout + result.stderr)
        commands = self.commands()
        self.assertFalse(any(c[0] in ('truncate', 'zip') for c in commands))
        self.assertEqual((self.project / 'test.img').read_bytes(), b'untruncated image')
        self.assertFalse((self.project / 'test.zip').exists())

    def assert_detached(self):
        self.assertEqual(json.loads((self.root / 'state').read_text())['mounted'], [])
        self.assertIn(['losetup', '-d', '/dev/mockloop'], self.commands())

    def test_success_checks_geometry_before_truncating_and_packages(self):
        result = self.build()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        commands = self.commands()
        truncate = next(i for i, c in enumerate(commands) if c[0] == 'truncate')
        resize = next(i for i, c in enumerate(commands) if c[0] == 'sfdisk')
        self.assertTrue(any(c[0] == 'parted' and 'print' in c for c in commands[resize:truncate]))
        self.assertTrue((self.project / 'test.zip').exists())
        detach = commands.index(['losetup', '-d', '/dev/mockloop'])
        package = next(i for i, c in enumerate(commands) if c[0] == 'zip')
        self.assertLess(detach, package)
        self.assertEqual(list(self.project.glob('.package.*')), [])
        self.assert_detached()

    def test_resize_failure_keeps_untruncated_image(self):
        self.assert_failed_before_packaging(self.build(RESIZE_RESULT='7'), 1)
        self.assert_detached()

    def test_silent_resize_noop_keeps_untruncated_image(self):
        self.assert_failed_before_packaging(self.build(RESIZE_NOOP='1'), 1)
        self.assert_detached()

    def test_missing_partition_keeps_untruncated_image(self):
        self.assert_failed_before_packaging(self.build(MISSING_PARTITION='1'), 1)
        self.assert_detached()

    def test_chroot_failure_unmounts_children_before_root_and_detaches(self):
        self.assert_failed_before_packaging(self.build(CHROOT_RESULT='23'), 23)
        self.assert_detached()

    def test_partial_mount_failure_is_cleaned_before_detaching(self):
        self.assert_failed_before_packaging(self.build(SYS_MOUNT_RESULT='19'), 19)
        self.assert_detached()

    def test_busy_chroot_keeps_loop_attached_and_does_not_package(self):
        result = self.build(FAIL_UNMOUNT='1')
        self.assert_failed_before_packaging(result, 1)
        self.assertIn('Mounts remain; keeping /dev/mockloop attached', result.stderr)
        self.assertNotIn(['losetup', '-d', '/dev/mockloop'], self.commands())

    def test_fsck_error_does_not_truncate_or_package(self):
        self.assert_failed_before_packaging(self.build(FSCK_RESULT='4'), 4)
        self.assert_detached()

    def test_termination_cleans_mounts_and_preserves_signal_exit_status(self):
        self.assert_failed_before_packaging(self.build(SEND_TERM='1'), 143)
        self.assert_detached()

    def test_loop_detach_failure_is_reported(self):
        previous = self.project / 'test.zip'
        previous.write_bytes(b'previous archive')
        result = self.build(DETACH_RESULT='5')
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertFalse(any(c[0] == 'zip' for c in self.commands()))
        self.assertEqual(previous.read_bytes(), b'previous archive')

    def assert_previous_zip_survives(self, **overrides):
        previous = self.project / 'test.zip'
        previous.write_bytes(b'previous archive')
        timestamp = previous.stat().st_mtime_ns
        result = self.build(**overrides)
        self.assertEqual(previous.read_bytes(), b'previous archive')
        self.assertEqual(previous.stat().st_mtime_ns, timestamp)
        self.assertEqual(list(self.project.glob('.package.*')), [])
        self.assert_detached()
        return result

    def test_zip_failure_preserves_previous_archive_and_cleans_staging(self):
        result = self.assert_previous_zip_survives(ZIP_RESULT='12')
        self.assertEqual(result.returncode, 12, result.stdout + result.stderr)

    def test_zip_failure_does_not_publish_first_archive(self):
        result = self.build(ZIP_RESULT='12')
        self.assertEqual(result.returncode, 12, result.stdout + result.stderr)
        self.assertFalse((self.project / 'test.zip').exists())
        self.assertEqual(list(self.project.glob('.package.*')), [])
        self.assert_detached()

    def test_termination_during_zip_preserves_previous_archive(self):
        result = self.assert_previous_zip_survives(ZIP_SEND_TERM='1')
        self.assertEqual(result.returncode, 143, result.stdout + result.stderr)

    def test_publish_failure_preserves_previous_archive(self):
        result = self.assert_previous_zip_survives(PUBLISH_RESULT='13')
        self.assertEqual(result.returncode, 13, result.stdout + result.stderr)

    def test_real_zip_replaces_archive_without_obsolete_entries(self):
        # Use the real ZIP tool to exercise its update behavior and verify the
        # consumer receives exactly one image and its matching checksum.
        (self.bin / 'zip').unlink()
        with zipfile.ZipFile(self.project / 'test.zip', 'w') as archive:
            archive.writestr('obsolete.img', b'old image')
            archive.writestr('obsolete.txt', b'old manifest')
        result = self.build()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        with zipfile.ZipFile(self.project / 'test.zip') as archive:
            self.assertIsNone(archive.testzip())
            self.assertEqual(set(archive.namelist()),
                             {'test.img', 'test.img.sha256', 'manifest-test.txt'})
            expected = hashlib.sha256(archive.read('test.img')).hexdigest()
            self.assertEqual(archive.read('test.img.sha256').decode(), f'{expected}  test.img\n')
            self.assertEqual(archive.read('manifest-test.txt'), b'test manifest\n')
        self.assertEqual(list(self.project.glob('.package.*')), [])
        self.assert_detached()


if __name__ == '__main__':
    unittest.main()
