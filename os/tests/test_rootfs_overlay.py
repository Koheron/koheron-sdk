"""Run the overlay payload in a disposable native chroot with fake systemctl.

Requires root and native Bash/coreutils (no disks, mounts or running services).
"""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest


OS_PATH = Path(__file__).resolve().parents[1]
PAYLOAD = OS_PATH / 'scripts/chroot_overlay.sh'
SERVICES = (
    'uwsgi', 'uwsgi.socket', 'grow-rootfs-once.service',
    'unzip-default-instrument', 'koheron-server', 'koheron-server-init',
    'nginx', 'systemd-networkd.service', 'systemd-resolved.service',
    'systemd-timesyncd.service',
)


@unittest.skipUnless(os.geteuid() == 0, 'Run as root inside the SDK build container')
class RootfsOverlayTest(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        # Copy native executables and their libraries into a small private
        # rootfs. Nothing from the builder's /etc is writable in this chroot.
        for name in ('bash', 'rm', 'install', 'ln', 'sed', 'cat'):
            executable = Path(shutil.which(name))
            dependencies = subprocess.check_output(['ldd', str(executable)], text=True)
            for source in (executable, *(Path(path) for path in
                            re.findall(r'/[^\s()]+', dependencies))):
                destination = self.root / source.relative_to('/')
                destination.parent.mkdir(parents=True, exist_ok=True)
                if not destination.exists():
                    shutil.copy2(source, destination)
        (self.root / 'bin').mkdir(exist_ok=True)
        if not (self.root / 'bin/bash').exists():
            (self.root / 'bin/bash').symlink_to('/usr/bin/bash')
        for path in ('etc/network/interfaces.d', 'etc/ssh', 'usr/bin'):
            (self.root / path).mkdir(parents=True, exist_ok=True)
        (self.root / 'etc/network/interfaces').write_text('old network config\n')
        (self.root / 'etc/network/interfaces.d/old').write_text('old network config\n')
        (self.root / 'etc/resolv.conf').write_text('old resolver config\n')
        (self.root / 'etc/ssh/sshd_config').write_text('#PermitRootLogin prohibit-password\n')
        shutil.copy2(PAYLOAD, self.root / 'chroot_overlay.sh')
        systemctl = self.root / 'usr/bin/systemctl'
        systemctl.write_text('''\
#!/bin/bash
printf '%s\\n' "$*" >> /commands
if [ "$2" = "${FAIL_SERVICE:-}" ]; then
    echo "Cannot enable $2" >&2
    exit 42
fi
''')
        systemctl.chmod(0o755)

    def run_payload(self, fail_service=''):
        return subprocess.run(
            [shutil.which('chroot'), str(self.root), '/bin/bash', '--noprofile', '--norc', '/chroot_overlay.sh'],
            env={'PATH': '/usr/bin:/bin', 'FAIL_SERVICE': fail_service},
            capture_output=True, text=True, timeout=10)

    def commands(self):
        return (self.root / 'commands').read_text().splitlines()

    def test_success_configures_network_and_enables_required_services(self):
        result = self.run_payload()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(set(self.commands()), {f'enable {name}' for name in SERVICES})
        for interface in ('end0', 'end1'):
            self.assertIn('DHCP=ipv4', (self.root / f'etc/systemd/network/10-{interface}.network').read_text())
        self.assertEqual(os.readlink(self.root / 'etc/resolv.conf'),
                         '../run/systemd/resolve/stub-resolv.conf')
        self.assertFalse((self.root / 'etc/network/interfaces').exists())
        self.assertEqual(list((self.root / 'etc/network/interfaces.d').iterdir()), [])

    def test_required_service_failure_stops_payload_and_preserves_error(self):
        for service in SERVICES:
            with self.subTest(service=service):
                (self.root / 'commands').unlink(missing_ok=True)
                result = self.run_payload(fail_service=service)
                self.assertEqual(result.returncode, 42, result.stdout + result.stderr)
                self.assertIn(f'Cannot enable {service}', result.stderr)
                self.assertEqual(self.commands()[-1], f'enable {service}')


if __name__ == '__main__':
    unittest.main()
