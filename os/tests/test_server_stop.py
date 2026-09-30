"""Test the instrument stop policy with mock processes in a user systemd manager.

Run: python3 -m unittest discover -s os/tests -p test_server_stop.py -v
The timeout case takes 30 seconds. Hardware boot dependencies and ExecStart are
replaced; the checked-in service's remaining [Service] settings are retained.
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


UNIT = Path(__file__).resolve().parents[1] / 'systemd/koheron-server.service'

PROBE = '''\
import ctypes
import os
from pathlib import Path
import signal
import socket
import sys
import time

ctypes.CDLL(None).prctl(15, b"khr-stop-probe", 0, 0, 0)
log = Path(sys.argv[1])
mode = sys.argv[2]

def record(event):
    with log.open("a") as stream:
        stream.write(event + "\\n")

def stop(signum, frame):
    record("interrupted")
    time.sleep(0.25)
    record("cleaned")
    sys.exit(0)

signal.signal(signal.SIGINT, signal.SIG_IGN if mode == "hung" else stop)
record("started")
address = os.environ.get("NOTIFY_SOCKET")
if address:
    with socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM) as connection:
        connection.connect("\\0" + address[1:] if address.startswith("@") else address)
        connection.sendall(b"READY=1")
while True:
    time.sleep(0.05)
'''


class ServerStopTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not shutil.which('systemctl') or not os.environ.get('XDG_RUNTIME_DIR'):
            raise unittest.SkipTest('Requires an active user systemd manager')
        result = subprocess.run(['systemctl', '--user', 'show-environment'],
                                capture_output=True, timeout=10)
        if result.returncode:
            raise unittest.SkipTest('Requires an active user systemd manager')

    def test_graceful_stop_restart_isolation_and_timeout(self):
        service = configparser.ConfigParser(interpolation=None)
        service.optionxform = str
        service.read(UNIT)
        # Never execute a legacy name-based stop command on the test machine.
        self.assertFalse(service.has_option('Service', 'ExecStop'))
        with tempfile.TemporaryDirectory(prefix='koheron-stop-') as directory:
            root = Path(directory)
            probe = root / 'probe.py'
            probe.write_text(PROBE)
            events = root / 'events'
            unrelated_events = root / 'unrelated-events'
            name = 'koheron-stop-test-' + uuid.uuid4().hex + '.service'
            unit_path = Path(os.environ['XDG_RUNTIME_DIR']) / 'systemd/user' / name
            unit_path.parent.mkdir(parents=True, exist_ok=True)
            unrelated = subprocess.Popen([sys.executable, str(probe),
                                          str(unrelated_events), 'unrelated'])

            def systemctl(*args, check=True):
                return subprocess.run(['systemctl', '--user', *args],
                                      capture_output=True, text=True,
                                      timeout=45, check=check)

            def write_unit(mode):
                unit = configparser.ConfigParser(interpolation=None)
                unit.optionxform = str
                unit['Unit'] = {'Description': 'Koheron stop policy regression test'}
                unit['Service'] = dict(service['Service'])
                unit['Service']['ExecStart'] = (
                    f'"{sys.executable}" "{probe}" "{events}" {mode}')
                buffer = io.StringIO()
                unit.write(buffer, space_around_delimiters=False)
                unit_path.write_text(buffer.getvalue())
                systemctl('daemon-reload')

            def properties(*keys):
                result = systemctl('show', '--all', name,
                                   *(f'--property={key}' for key in keys))
                values = dict(line.split('=', 1) for line in result.stdout.splitlines())
                # systemctl omits empty command-array properties such as ExecStop.
                return {key: values.get(key, '') for key in keys}

            try:
                write_unit('graceful')
                systemctl('start', name)
                policy = properties('KillSignal', 'KillMode', 'SendSIGKILL',
                                    'TimeoutStopUSec', 'ExecStop')
                self.assertEqual(policy, {'KillSignal': '2', 'KillMode': 'control-group',
                                          'SendSIGKILL': 'yes', 'TimeoutStopUSec': '30s',
                                          'ExecStop': ''})
                self.assertIsNone(unrelated.poll())
                main_pid = properties('MainPID')['MainPID']
                self.assertEqual(Path(f'/proc/{main_pid}/comm').read_text(),
                                 Path(f'/proc/{unrelated.pid}/comm').read_text())
                systemctl('stop', name)
                self.assertEqual(events.read_text().splitlines(),
                                 ['started', 'interrupted', 'cleaned'])
                self.assertEqual(properties('Result', 'ExecMainStatus'),
                                 {'Result': 'success', 'ExecMainStatus': '0'})
                self.assertIsNone(unrelated.poll())

                systemctl('start', name)
                systemctl('restart', name)
                self.assertEqual(events.read_text().splitlines().count('cleaned'), 2)
                self.assertEqual(events.read_text().splitlines().count('started'), 3)
                systemctl('stop', name)
                self.assertEqual(events.read_text().splitlines().count('cleaned'), 3)

                write_unit('hung')
                systemctl('start', name)
                started = time.monotonic()
                systemctl('stop', name)
                elapsed = time.monotonic() - started
                self.assertGreaterEqual(elapsed, 29)
                self.assertLess(elapsed, 40)
                self.assertEqual(properties('Result', 'ExecMainStatus', 'MainPID'),
                                 {'Result': 'timeout', 'ExecMainStatus': '9', 'MainPID': '0'})
                self.assertIsNone(unrelated.poll())
                self.assertEqual(unrelated_events.read_text().splitlines(), ['started'])
            finally:
                systemctl('stop', name, check=False)
                systemctl('reset-failed', name, check=False)
                unit_path.unlink(missing_ok=True)
                systemctl('daemon-reload')
                unrelated.terminate()
                unrelated.wait(timeout=5)


if __name__ == '__main__':
    unittest.main()
