"""Opt-in board integration using private systemd units, nginx and instrument files.

Stage binaries, libraries, the API service/socket and nginx configurations in
/tmp/native-management. No production unit is stopped or reconfigured.
"""
import hashlib
import io
import json
import os
from pathlib import Path
import signal
import stat
import subprocess
import time
import urllib.error
import urllib.parse
import urllib.request
import zipfile

ROOT = Path('/tmp/native-management')
API = 'koheron-native-test'
INSTRUMENT = 'koheron-native-instrument-test.service'
LED = 'koheron-native-led-test.service'


def ctl(*arguments, check=True):
    return subprocess.run(['systemctl', *arguments], check=check, text=True, capture_output=True, timeout=15)


def pids():
    return {unit: ctl('show', '-p', 'MainPID', '--value', unit).stdout.strip()
        for unit in ('koheron-server', 'uwsgi', 'nginx')}


def package(name, fail=False, extra=None):
    script = ('#!/bin/sh\necho native-instrument-' + name + '\n' +
        ('exit 23\n' if fail else 'exec /usr/bin/systemd-notify --ready --exec ";" -- /bin/sleep infinity\n')).encode()
    data = io.BytesIO()
    with zipfile.ZipFile(data, 'w') as archive:
        for member, payload in {'serverd': script, 'version': name.encode(),
            'pl.dtbo': b'fixture', 'fixture.bit.bin': b'fixture',
            'drivers.json': b'[{"class":"Common","id":2,"functions":[]}]', **(extra or {})}.items():
            info = zipfile.ZipInfo(member)
            info.external_attr = (stat.S_IFREG | (0o755 if member == 'serverd' else 0o644)) << 16
            archive.writestr(info, payload)
    return data.getvalue()


def request(path, data=None, headers=None):
    query = urllib.request.Request('http://127.0.0.1:18087' + path, data=data, headers=headers or {})
    try: response = urllib.request.urlopen(query, timeout=10)
    except urllib.error.HTTPError as error: response = error
    with response:
        body = response.read()
        return response.status, json.loads(body) if response.headers.get_content_type() == 'application/json' else body.decode()


def upload(name, data):
    boundary = 'koheron-native-board-7324'
    body = (f'--{boundary}\r\nContent-Disposition: form-data; name="{name}.zip"; filename="{name}.zip"\r\n'
        'Content-Type: application/zip\r\n\r\n').encode() + data + f'\r\n--{boundary}--\r\n'.encode()
    return request('/api/instruments/upload', body, {'Content-Type': 'multipart/form-data; boundary=' + boundary})


def main():
    before = pids()
    production = Path('/usr/local/api/app/__init__.py')
    digest = hashlib.sha256(production.read_bytes()).hexdigest()
    units = []
    nginx = None
    checks = []
    def passed(name): checks.append(name); print('PASS: ' + name, flush=True)
    def write_unit(name, contents):
        path = Path('/run/systemd/system') / name
        if path.exists(): raise RuntimeError('Test unit already exists: ' + name)
        path.write_text(contents); units.append(path)
    live, store = ROOT / 'integration-live', ROOT / 'integration-store'
    live.mkdir(exist_ok=True); store.mkdir(exist_ok=True)
    old = package('old')
    (store / 'old.zip').write_bytes(old); (store / 'default').write_text('old.zip\n')
    with zipfile.ZipFile(io.BytesIO(old)) as archive: archive.extractall(live)
    (live / 'serverd').chmod(0o755); (live / '.instrument-name').write_text('old\n')
    environment = f'Environment=LD_LIBRARY_PATH={ROOT}/lib\n'
    try:
        write_unit(INSTRUMENT, '[Unit]\nDescription=Isolated native installer probe\n[Service]\nType=notify\n'
            f'ExecStart={live}/serverd\nTimeoutStartSec=5s\nTimeoutStopSec=5s\n')
        write_unit(LED, '[Service]\nType=oneshot\nExecStart=/bin/true\n')
        service = (ROOT / 'koheron-api.service').read_text().replace('Sockets=koheron-api.socket', f'Sockets={API}.socket')
        service = service.replace('ExecStart=/usr/local/api/koheron-api', environment +
            f'ExecStart={ROOT}/koheron-api --instruments {store} --live {live} --unit {INSTRUMENT} --led-unit {LED}')
        write_unit(API + '.service', service)
        socket_unit = (ROOT / 'koheron-api.socket').read_text().replace('/run/koheron-api/app.sock', str(ROOT / 'http.sock'))
        write_unit(API + '.socket', socket_unit)
        ctl('daemon-reload'); ctl('start', INSTRUMENT)
        start = time.monotonic(); ctl('start', API + '.socket', API + '.service')
        activation_ms = (time.monotonic() - start) * 1000
        global_config = (ROOT / 'nginx.conf').read_text().replace('/run/nginx.pid', str(ROOT / 'nginx.pid'))
        global_config = global_config.replace('/run/nginx/', str(ROOT / 'nginx-tmp') + '/')
        global_config = global_config.replace('include             /etc/nginx/conf.d/*.conf;', '')
        global_config = global_config.replace('/etc/nginx/sites-enabled/*', str(ROOT / 'server.conf'))
        (ROOT / 'nginx-tmp').mkdir(exist_ok=True)
        (ROOT / 'nginx-private.conf').write_text(global_config)
        server = (ROOT / 'nginx-server.conf').read_text().replace('listen                      80;', 'listen 127.0.0.1:18087;')
        server = server.replace('/run/koheron-api/app.sock', str(ROOT / 'http.sock'))
        (ROOT / 'server.conf').write_text(server)
        arguments = ['nginx', '-p', str(ROOT), '-c', str(ROOT / 'nginx-private.conf')]
        subprocess.run([*arguments, '-t'], check=True, timeout=5)
        log = (ROOT / 'nginx-private.log').open('wb')
        nginx = subprocess.Popen([*arguments, '-g', 'daemon off;'], stdout=log, stderr=subprocess.STDOUT)
        deadline = time.monotonic() + 5
        while True:
            try:
                status, details = request('/api/instruments/details'); break
            except OSError:
                if time.monotonic() > deadline: raise
                time.sleep(0.02)
        assert status == 200 and details['live_instrument']['name'] == 'old'
        passed('systemd notify readiness and nginx HTTP Unix-socket proxy')
        assert upload('new', package('new'))[0] == 200
        assert request('/api/instruments/run/new')[0] == 200
        assert request('/api/instruments/details')[1]['live_instrument']['name'] == 'new'
        assert ctl('is-active', INSTRUMENT).stdout.strip() == 'active'
        passed('multipart upload and real systemd instrument activation')
        subprocess.run(['journalctl', '--sync'], check=True, timeout=5)
        startup = request('/api/logs/koheron/instrument/incr')[1]
        assert any('native-instrument-new' in entry['msg'] for entry in startup['entries'])
        passed('instrument cursor includes the first startup message of the new invocation')
        assert request('/api/instruments/commands/new')[1][0]['class'] == 'Common'
        passed('drivers.json download')
        old_pid = ctl('show', '-p', 'MainPID', '--value', INSTRUMENT).stdout
        assert upload('invalid', package('invalid', extra={'../escape': b'unsafe'}))[0] == 200
        assert request('/api/instruments/run/invalid')[0] == 500
        assert ctl('show', '-p', 'MainPID', '--value', INSTRUMENT).stdout == old_pid
        assert not (ROOT / 'escape').exists()
        passed('unsafe extraction fails before stopping the running service')
        assert upload('broken', package('broken', fail=True))[0] == 200
        assert request('/api/instruments/run/broken')[0] == 500
        assert request('/api/instruments/details')[1]['live_instrument']['name'] == 'new'
        assert ctl('is-active', INSTRUMENT).stdout.strip() == 'active'
        passed('failed readiness restores files and restarts the previous service')
        subprocess.run(['journalctl', '--sync'], check=True, timeout=5)
        restored = request('/api/logs/koheron/instrument/incr')[1]
        assert any('native-instrument-new' in entry['msg'] for entry in restored['entries'])
        passed('instrument log stream follows the restored service invocation')
        status, logs = request('/api/logs/koheron?lines=100')
        assert status == 200 and any('native-instrument-new' in entry['msg'] for entry in logs['entries'])
        bookmark = request('/api/logs/koheron/bookmark')[1]
        assert bookmark['cursor'] and isinstance(bookmark['ts'], int)
        assert request('/api/logs/koheron/incr?cursor=' + urllib.parse.quote(bookmark['cursor']))[1]['entries'] == []
        passed('real journal tail, timestamps, bookmark and incremental cursor')
        ctl('restart', API + '.service')
        assert request('/api/instruments/details')[1]['live_instrument']['name'] == 'new'
        passed('socket activation survives API restart and recovers loaded identity')
        ctl('stop', INSTRUMENT)
        assert request('/api/instruments/details')[1]['live_instrument'] is None
        passed('stopped instrument status')
        assert request('/api/instruments/delete/old')[1] == 'Default instrument cannot be removed'
        assert request('/api/instruments/delete/broken')[0] == 200
        passed('default protection and archive deletion')
        subprocess.run([str(ROOT / 'koheron-server-init'), '/run/koheron-server.sock'],
            env={**os.environ, 'LD_LIBRARY_PATH': str(ROOT / 'lib')}, check=True, timeout=5)
        passed('native LED RPC against the production FFT server')
        data = {'checks': checks, 'activation_ms': activation_ms, 'journal_sample': logs,
            'production_pids_before': before, 'production_api_hash': digest}
    finally:
        if nginx:
            nginx.send_signal(signal.SIGQUIT); nginx.wait(timeout=5); log.close()
        for path in units: ctl('stop', path.name, check=False)
        for path in units: ctl('reset-failed', path.name, check=False); path.unlink(missing_ok=True)
        ctl('daemon-reload')
    data.update(production_pids_after=pids(), production_api_unchanged=digest == hashlib.sha256(production.read_bytes()).hexdigest(),
        temporary_units_removed=all(not path.exists() for path in units))
    assert before == data['production_pids_after'] and data['production_api_unchanged']
    (ROOT / 'integration.json').write_text(json.dumps(data, indent=2) + '\n')
    passed('temporary services removed; production PIDs and API unchanged')


if __name__ == '__main__': main()
