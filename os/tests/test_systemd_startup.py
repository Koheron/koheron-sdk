"""Exercise boot ordering with real systemd and isolated, gated mock services.

No board access or system units: every live unit uses a unique user-unit name.
"""
import configparser
import io
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time
import unittest
import uuid

UNITS = Path(__file__).resolve().parents[1] / 'systemd'
EARLY = ('unzip-default-instrument.service', 'koheron-server.service',
         'nginx.service', 'uwsgi.service')
PROBE = '''\
import os, signal, socket, sys, time
from pathlib import Path
root, name, mode = Path(sys.argv[1]), sys.argv[2], sys.argv[3]
(root / (name + '.started')).touch()
while not (root / (name + '.release')).exists():
    time.sleep(0.01)
if (root / (name + '.fail')).exists():
    sys.exit(23)
if mode == 'oneshot':
    sys.exit(0)
if mode == 'forking':
    if os.fork():
        sys.exit(0)
else:
    address = os.environ['NOTIFY_SOCKET']
    with socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM) as s:
        s.connect('\\0' + address[1:] if address.startswith('@') else address)
        s.sendall(b'READY=1')
signal.signal(signal.SIGINT, lambda *_: sys.exit(0))
while True:
    signal.pause()
'''


def read_unit(name):
    unit = configparser.ConfigParser(interpolation=None)
    unit.optionxform = str
    unit.read(UNITS / name)
    return unit


def write_unit(path, unit):
    out = io.StringIO()
    unit.write(out, space_around_delimiters=False)
    path.write_text(out.getvalue())


@unittest.skipUnless(shutil.which('systemctl'), 'requires systemd tools')
class EnablementTest(unittest.TestCase):
    def test_reenable_removes_legacy_basic_links_and_is_idempotent(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            units = root / 'etc/systemd/system'
            legacy = units / 'basic.target.wants'
            legacy.mkdir(parents=True)
            for name in EARLY:
                shutil.copy2(UNITS / name, units / name)
                (legacy / name).symlink_to('../' + name)
            for _ in range(2):
                result = subprocess.run(['systemctl', '--root=' + str(root),
                                         'reenable', *EARLY],
                                        capture_output=True, text=True, timeout=10)
                self.assertEqual(result.returncode, 0, result.stderr)
                for name in EARLY:
                    self.assertFalse((legacy / name).is_symlink(), name)
                    self.assertTrue((units / 'multi-user.target.wants' / name).is_symlink(), name)


class StartupOrderingTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not shutil.which('systemctl') or not os.environ.get('XDG_RUNTIME_DIR'):
            raise unittest.SkipTest('requires an active user systemd manager')
        result = subprocess.run(['systemctl', '--user', 'show-environment'],
                                capture_output=True, timeout=10)
        if result.returncode:
            raise unittest.SkipTest('requires an active user systemd manager')

    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='koheron-startup-')
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.prefix = 'koheron-boot-test-' + uuid.uuid4().hex + '-'
        self.unit_dir = Path(os.environ['XDG_RUNTIME_DIR']) / 'systemd/user'
        self.unit_dir.mkdir(parents=True, exist_ok=True)
        self.files = []
        self.addCleanup(self.cleanup_units)
        probe = self.root / 'probe.py'
        probe.write_text(PROBE)
        services = (*EARLY, 'koheron-server-init.service')
        wanted = {'basic.target': [], 'multi-user.target': ['ssh-probe.service']}
        for name in services:
            unit = read_unit(name)
            for target in unit['Install'].get('WantedBy', '').split():
                wanted.setdefault(target, []).append(name)
            for key in ('After', 'Before', 'Wants', 'Requires', 'Conflicts'):
                if key in unit['Unit']:
                    unit['Unit'][key] = ' '.join(self.name(n) for n in unit['Unit'][key].split())
            service = unit['Service']
            mode = service['Type']
            sockets = service.get('Sockets')
            # Keep the real service type and unit dependencies. Replace only
            # process/runtime settings that require the board or root.
            unit['Service'] = {'Type': mode, 'ExecStart': f'{sys.executable} {probe} {self.root} {name} {mode}',
                               'TimeoutStartSec': '20s', 'TimeoutStopSec': '2s'}
            if mode == 'notify':
                unit['Service']['NotifyAccess'] = 'main'
            if sockets:
                unit['Service']['Sockets'] = ' '.join(self.name(n) for n in sockets.split())
            del unit['Install']
            self.put(name, unit)
        socket_unit = read_unit('uwsgi.socket')
        socket_unit['Socket'] = {'ListenStream': str(self.root / 'api.sock')}
        del socket_unit['Install']
        self.put('uwsgi.socket', socket_unit)
        for name in ('sysinit.target', 'local-fs.target', 'tmp.mount', 'shutdown.target'):
            self.put(name, {'Unit': {'DefaultDependencies': 'no'}})
        for name in ('systemd-remount-fs.service', 'systemd-tmpfiles-setup.service'):
            self.put(name, {'Unit': {'DefaultDependencies': 'no'},
                            'Service': {'Type': 'oneshot', 'ExecStart': '/bin/true', 'RemainAfterExit': 'yes'}})
        for target, prerequisites in (('basic.target', ('sysinit.target', 'local-fs.target', 'tmp.mount')),
                                      ('multi-user.target', ('basic.target',))):
            self.put(target, {'Unit': {'Requires': ' '.join(self.name(n) for n in prerequisites),
                                      'After': ' '.join(self.name(n) for n in prerequisites),
                                      'Wants': ' '.join(self.name(n) for n in wanted[target])}})
        self.put('ssh-probe.service', {'Unit': {'After': self.name('basic.target')},
                                      'Service': {'Type': 'oneshot', 'ExecStart': '/bin/true',
                                                  'RemainAfterExit': 'yes'}})
        self.ctl('daemon-reload')

    def name(self, name):
        # A mount prerequisite is represented by an inert target; no mounts.
        return self.prefix + ('tmp-mount.target' if name == 'tmp.mount' else name)

    def put(self, name, contents):
        if not isinstance(contents, configparser.ConfigParser):
            unit = configparser.ConfigParser(interpolation=None)
            unit.optionxform = str
            unit.read_dict(contents)
        else:
            unit = contents
        path = self.unit_dir / self.name(name)
        write_unit(path, unit)
        self.files.append(path)

    def ctl(self, *args, check=True):
        return subprocess.run(['systemctl', '--user', *args], check=check,
                              capture_output=True, text=True, timeout=10)

    def state(self, name):
        return self.ctl('show', self.name(name), '--property=ActiveState', '--value').stdout.strip()

    def wait_for(self, condition):
        deadline = time.monotonic() + 8
        while not condition():
            if time.monotonic() >= deadline:
                self.fail('Timed out waiting for boot ordering condition')
            time.sleep(0.02)

    def marker(self, name, kind):
        return self.root / (name + '.' + kind)

    def cleanup_units(self):
        if not self.files:
            return
        names = [p.name for p in self.files]
        self.ctl('stop', *names, check=False)
        self.ctl('reset-failed', *names, check=False)
        for path in self.files:
            path.unlink(missing_ok=True)
        self.ctl('daemon-reload')

    def test_slow_services_do_not_block_basic_but_preserve_readiness_and_shutdown(self):
        self.ctl('start', '--no-block', self.name('multi-user.target'))
        self.wait_for(lambda: self.state('ssh-probe.service') == 'active')
        for name in ('unzip-default-instrument.service', 'uwsgi.service', 'nginx.service'):
            self.wait_for(lambda: self.marker(name, 'started').exists())
            self.assertEqual(self.state(name), 'activating')
        server = 'koheron-server.service'
        led = 'koheron-server-init.service'
        self.assertFalse(self.marker(server, 'started').exists())
        self.marker('unzip-default-instrument.service', 'release').touch()
        self.wait_for(lambda: self.marker(server, 'started').exists())
        self.assertEqual(self.state(server), 'activating')
        self.assertFalse(self.marker(led, 'started').exists())
        for name in (server, 'uwsgi.service', 'nginx.service'):
            self.marker(name, 'release').touch()
            self.wait_for(lambda: self.state(name) == 'active')
        self.wait_for(lambda: self.marker(led, 'started').exists())
        self.ctl('start', self.name('shutdown.target'))
        for name in EARLY:
            self.assertEqual(self.state(name), 'inactive', name)

    def test_extraction_failure_blocks_instrument_but_not_basic_services(self):
        extraction = 'unzip-default-instrument.service'
        self.marker(extraction, 'fail').touch()
        self.marker(extraction, 'release').touch()
        self.ctl('start', '--no-block', self.name('multi-user.target'))
        self.wait_for(lambda: self.state(extraction) == 'failed')
        self.wait_for(lambda: self.state('ssh-probe.service') == 'active')
        self.assertFalse(self.marker('koheron-server.service', 'started').exists())
        self.assertFalse(self.marker('koheron-server-init.service', 'started').exists())


if __name__ == '__main__':
    unittest.main()
