"""Black-box fixtures for the compiled board runtime; Python runs only on the host."""
import io
import json
import os
from pathlib import Path
import socket
import stat
import subprocess
import tempfile
import time
import unittest
import urllib.error
import urllib.request
import zipfile

BIN_DIR = Path(os.environ.get('NATIVE_API_BIN_DIR', 'tmp/native-api/amd64')).resolve()

SERVICE = '''#!/usr/bin/python3
import json, os, sys, time
from pathlib import Path
root = Path(os.environ['NATIVE_TEST_ROOT'])
path = root / 'service.json'
state = json.loads(path.read_text())
action, unit = sys.argv[1], sys.argv[-1]
with (root / 'operations').open('a') as output:
    output.write(action + ' ' + unit + '\\n')
result = 0
if unit != 'koheron-server.service':
    result = int(state.get('fail_led', False))
elif action == 'is-active':
    if (root / 'hold-status').exists():
        (root / 'status-entered').touch()
        while (root / 'hold-status').exists(): time.sleep(.005)
    result = 0 if state['active'] else 3
elif action == 'stop':
    result = int(state.get('fail_stop', False))
    if not result: state['active'] = False
elif action == 'start':
    name = (root / 'live/.instrument-name').read_text().strip()
    result = int(state.get('fail_' + name, False))
    if not result: state['active'] = True
if action != 'is-active':
    path.write_text(json.dumps(state))
sys.exit(result)
'''

def archive(version='2', executable=True, legacy=False, extra=None, compression=zipfile.ZIP_STORED):
    members = {'serverd': b'new executable',
               **({'new.bit': b'bitstream'} if legacy else {'pl.dtbo': b'overlay', 'new.bit.bin': b'bitstream'})}
    if version is not None:
        members['version'] = version.encode()
    members.update(extra or {})
    output = io.BytesIO()
    with zipfile.ZipFile(output, 'w', compression=compression) as package:
        for name, data in members.items():
            member = zipfile.ZipInfo(name)
            member.compress_type = compression
            member.external_attr = (stat.S_IFREG | (0o755 if name == 'serverd' and executable else 0o644)) << 16
            package.writestr(member, data)
    return output.getvalue()


class NativeFixture(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not (BIN_DIR / 'koheron-api').is_file():
            raise RuntimeError('Build native API binaries and set NATIVE_API_BIN_DIR before running tests')

    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='koheron-native-test-')
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.store, self.live = self.root / 'store', self.root / 'live'
        self.store.mkdir(); self.live.mkdir()
        self.original = archive(version='1', extra={'serverd': b'old executable'})
        (self.store / 'old.zip').write_bytes(self.original)
        (self.store / 'default').write_text('old.zip\n')
        (self.live / '.instrument-name').write_text('old\n')
        (self.live / 'version').write_text('loaded-version')
        (self.live / 'serverd').write_bytes(b'old executable')
        self.package = self.store / 'new.zip'
        self.package.write_bytes(archive())
        self.state = {'active': True}
        self.save_state()
        self.systemctl = self.root / 'systemctl'
        self.systemctl.write_text(SERVICE); self.systemctl.chmod(0o755)
        self.environment = {**os.environ, 'NATIVE_TEST_ROOT': str(self.root)}
        self.process = None

    def save_state(self, **updates):
        self.state.update(updates)
        (self.root / 'service.json').write_text(json.dumps(self.state))

    def load_state(self):
        return json.loads((self.root / 'service.json').read_text())

    def operations(self):
        path = self.root / 'operations'
        return path.read_text().splitlines() if path.exists() else []

    def start_api(self):
        with socket.socket() as probe:
            probe.bind(('127.0.0.1', 0)); self.port = probe.getsockname()[1]
        self.output = (self.root / 'api.log').open('wb')
        self.addCleanup(self.output.close)
        self.process = subprocess.Popen([str(BIN_DIR / 'koheron-api'), '--port', str(self.port),
            '--instruments', str(self.store), '--live', str(self.live), '--systemctl', str(self.systemctl),
            '--manifest', str(self.root / 'manifest'), '--release', str(self.root / 'release')],
            env=self.environment, stdout=self.output, stderr=subprocess.STDOUT)
        self.addCleanup(self.stop_api)
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            if self.process.poll() is not None:
                self.fail((self.root / 'api.log').read_text())
            try:
                self.request('/api/system/build'); return
            except OSError:
                time.sleep(0.01)
        self.fail('Native API did not become ready')

    def stop_api(self):
        if self.process and self.process.poll() is None:
            self.process.terminate()
            try: self.process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.process.kill(); self.process.wait(timeout=3)
                self.fail('Native API failed to stop gracefully')

    def request(self, path, data=None, headers=None, method=None):
        request = urllib.request.Request(f'http://127.0.0.1:{self.port}' + path,
            data=data, headers=headers or {}, method=method)
        try:
            response = urllib.request.urlopen(request, timeout=5)
        except urllib.error.HTTPError as error:
            response = error
        with response:
            return response.status, response.read(), response.headers

    def upload(self, data, name='new.zip'):
        boundary = 'koheron-test-boundary-8371'
        body = (f'--{boundary}\r\nContent-Disposition: form-data; name="{name}"; filename="{name}"\r\n'
                'Content-Type: application/zip\r\n\r\n').encode() + data + f'\r\n--{boundary}--\r\n'.encode()
        return self.request('/api/instruments/upload', body, {'Content-Type': 'multipart/form-data; boundary=' + boundary})

    def install(self):
        return subprocess.run([str(BIN_DIR / 'koheron-install'), str(self.package), str(self.live),
            '--systemctl', str(self.systemctl)], env=self.environment, capture_output=True, text=True, timeout=5)

    def assert_old(self):
        self.assertEqual((self.live / 'serverd').read_bytes(), b'old executable')
        self.assertEqual((self.live / '.instrument-name').read_text().strip(), 'old')
