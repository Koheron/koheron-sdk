"""Exercise offline image hygiene and build failure paths without real mounts."""
import os
from pathlib import Path
import subprocess
import tarfile
import tempfile
import unittest


OS_PATH = Path(__file__).resolve().parents[1]
FINALIZE = OS_PATH / 'scripts/finalize_rootfs.sh'
BUILD = OS_PATH / 'scripts/build_base_rootfs_tar.sh'


class FinalizeRootfsTest(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.files = (
            'etc/machine-id', 'var/lib/dbus/machine-id',
            'etc/ssh/ssh_host_rsa_key', 'etc/ssh/ssh_host_rsa_key.pub',
            'var/lib/systemd/random-seed', 'var/lib/systemd/credential.secret',
            'etc/dpkg/dpkg.cfg.d/02_nofsync', 'usr/bin/qemu-arm-static',
            'chroot.sh', 'chroot_overlay.sh',
        )
        for name in (*self.files, 'etc/ssh/sshd_config',
                     'etc/dpkg/dpkg.cfg.d/01_nodoc', 'usr/bin/board-tool'):
            target = self.root / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text('keep or reset\n')

    def finalize(self, qemu='qemu-arm-static'):
        return subprocess.run(['bash', str(FINALIZE), str(self.root), qemu],
                              capture_output=True, text=True, timeout=5)

    def test_resets_identity_and_removes_only_build_files(self):
        for _ in range(2):
            result = self.finalize()
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual((self.root / 'etc/machine-id').read_bytes(), b'')
            self.assertEqual(os.readlink(self.root / 'var/lib/dbus/machine-id'),
                             '/etc/machine-id')
            for name in self.files[2:]:
                self.assertFalse((self.root / name).exists(), name)
            for name in ('etc/ssh/sshd_config', 'etc/dpkg/dpkg.cfg.d/01_nodoc',
                         'usr/bin/board-tool'):
                self.assertEqual((self.root / name).read_text(), 'keep or reset\n')

    def test_machine_id_symlink_does_not_modify_its_target(self):
        target = self.root / 'identity-to-preserve'
        target.write_text('original\n')
        machine_id = self.root / 'etc/machine-id'
        machine_id.unlink()
        machine_id.symlink_to(target)
        self.assertEqual(self.finalize().returncode, 0)
        self.assertEqual(target.read_text(), 'original\n')
        self.assertFalse(machine_id.is_symlink())

    def test_aarch64_helper_is_removed(self):
        helper = self.root / 'usr/bin/qemu-aarch64-static'
        helper.touch()
        self.assertEqual(self.finalize('qemu-aarch64-static').returncode, 0)
        self.assertFalse(helper.exists())

    def test_rejects_invalid_helper_before_changing_rootfs(self):
        self.assertNotEqual(self.finalize('../../board-tool').returncode, 0)
        self.assertEqual((self.root / 'etc/machine-id').read_text(), 'keep or reset\n')

    def test_refuses_live_root(self):
        result = subprocess.run(['bash', str(FINALIZE), '/', 'qemu-arm-static'],
                                capture_output=True, timeout=5)
        self.assertNotEqual(result.returncode, 0)


class BaseRootfsBuildTest(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.work = self.root / 'work'
        self.work.mkdir()
        source = self.root / 'source'
        for name in ('etc', 'usr/bin', 'proc', 'sys', 'dev', 'run'):
            (source / name).mkdir(parents=True, exist_ok=True)
        self.archive = self.root / 'ubuntu-base.tgz'
        with tarfile.open(self.archive, 'w:gz') as archive:
            archive.add(source, arcname='.')
        self.output = self.root / 'base-koheron.tgz'
        self.output.write_bytes(b'previous cache')
        self.qemu = self.root / 'qemu-arm-static'
        self.qemu.write_text('#!/bin/sh\nexit 0\n')
        self.qemu.chmod(0o755)
        bin_dir = self.root / 'bin'
        bin_dir.mkdir()
        mock = bin_dir / 'mock'
        mock.write_text('''\
#!/usr/bin/python3
import os
from pathlib import Path
import sys
command = Path(sys.argv[0]).name
args = sys.argv[1:]
if command == 'mount':
    if args[0] != '--make-rslave':
        (Path(args[-1]) / '.test-mounted').touch()
elif command == 'mountpoint':
    sys.exit(0 if (Path(args[-1]) / '.test-mounted').exists() else 1)
elif command == 'umount':
    if os.environ.get('FAIL_UNMOUNT'):
        sys.exit(17)
    (Path(args[-1]) / '.test-mounted').unlink()
elif command == 'chroot':
    root = Path(args[0])
    (root / 'etc/machine-id').write_text('builder-id\\n')
    (root / 'payload-ran').touch()
    sys.exit(int(os.environ.get('CHROOT_RESULT', '0')))
elif command == 'tar':
    if '-czf' in args and os.environ.get('FAIL_PACK'):
        Path(args[args.index('-czf') + 1]).write_bytes(b'incomplete archive')
        sys.exit(19)
    os.execv('/usr/bin/tar', ['tar', *args])
elif command != 'chown':
    raise RuntimeError(command)
''')
        mock.chmod(0o755)
        for name in ('mount', 'mountpoint', 'umount', 'chroot', 'tar', 'chown'):
            (bin_dir / name).symlink_to(mock)
        self.env = {**os.environ, 'PATH': str(bin_dir) + ':' + os.environ['PATH'],
                    'WORKDIR': str(self.work)}

    def build(self, **env):
        return subprocess.run(['bash', str(BUILD), str(self.archive),
                               str(self.output), str(self.qemu)],
                              capture_output=True, text=True,
                              env={**self.env, **env}, timeout=15)

    def assert_clean_failure(self, result, code):
        self.assertEqual(result.returncode, code, result.stdout + result.stderr)
        self.assertEqual(self.output.read_bytes(), b'previous cache')
        self.assertEqual(list(self.work.iterdir()), [])
        self.assertEqual(list(self.root.glob('base-koheron.tgz.tmp.*')), [])

    def test_success_publishes_clean_archive_and_removes_build_tree(self):
        result = self.build()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        with tarfile.open(self.output) as archive:
            names = archive.getnames()
            self.assertIn('./payload-ran', names)
            self.assertEqual(archive.extractfile('./etc/machine-id').read(), b'')
            self.assertNotIn('./chroot.sh', names)
            self.assertNotIn('./usr/bin/qemu-arm-static', names)
            self.assertFalse(any(name.endswith('.test-mounted') for name in names))
        self.assertEqual(list(self.work.iterdir()), [])
        self.assertEqual(self.output.stat().st_mode & 0o777, 0o644)

    def test_chroot_failure_keeps_previous_cache_and_cleans_tree(self):
        self.assert_clean_failure(self.build(CHROOT_RESULT='23'), 23)

    def test_pack_failure_keeps_previous_cache_and_removes_partial_archive(self):
        self.assert_clean_failure(self.build(FAIL_PACK='1'), 19)

    def test_unmount_failure_retains_tree_and_does_not_publish(self):
        result = self.build(FAIL_UNMOUNT='1')
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.output.read_bytes(), b'previous cache')
        self.assertIn('Rootfs mounts remain', result.stderr)
        trees = list(self.work.iterdir())
        self.assertEqual(len(trees), 1)
        self.assertTrue((trees[0] / 'dev/.test-mounted').exists())


if __name__ == '__main__':
    unittest.main()
