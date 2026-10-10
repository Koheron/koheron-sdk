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
import sys

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


def request(path, data=None, headers=None, method=None):
    query = urllib.request.Request('http://127.0.0.1:18087' + path, data=data, headers=headers or {}, method=method)
    try: response = urllib.request.urlopen(query, timeout=10)
    except urllib.error.HTTPError as error: response = error
    with response:
        body = response.read()
        return response.status, json.loads(body) if body and response.headers.get_content_type() == 'application/json' else body.decode()


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
        if '--preview' in sys.argv:
            server = server.replace('listen 127.0.0.1:18087;', 'listen 18087;')
            server = server.replace('/usr/local/www', str(ROOT / 'www'))
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
        if '--management-controls' in sys.argv:
            from test_native_management import WebSocket
            assert request('/api/instruments/preflight/new')[1]['ready']
            passed('native preflight verifies archive and staging capacity')
            files_before = {file.name: hashlib.sha256(file.read_bytes()).hexdigest() for file in live.iterdir() if file.is_file()}
            assert request('/api/instruments/control/start', b'')[0] == 200
            first_pid = ctl('show', '-p', 'MainPID', '--value', INSTRUMENT).stdout
            assert request('/api/instruments/control/restart', b'')[0] == 200
            assert ctl('show', '-p', 'MainPID', '--value', INSTRUMENT).stdout != first_pid
            assert request('/api/instruments/control/stop', b'')[0] == 200
            assert request('/api/system/status')[1]['current_instrument']['name'] == 'new'
            assert request('/api/instruments/control/start', b'')[0] == 200
            assert files_before == {file.name: hashlib.sha256(file.read_bytes()).hexdigest() for file in live.iterdir() if file.is_file()}
            passed('D-Bus start/stop/restart preserve loaded files')
            assert request('/api/instruments/default/new', b'')[0] == 200
            ctl('restart', API + '.service')
            assert next(item for item in request('/api/instruments/details')[1]['instruments'] if item['name'] == 'new')['is_default']
            assert request('/api/instruments/default/old', b'')[0] == 200
            passed('atomic boot selection persists across API restart')
            health = request('/api/system/status')[1]['health']
            assert health['memory']['available_bytes'] > 0 and health['uptime_seconds'] > 0
            assert health['instrument_service']['state'] == 'active'
            assert health['timing']['api_ready_monotonic_us'] > 0
            passed('real board RAM, storage, uptime and systemd readiness')
            stream = WebSocket(18087)
            try:
                assert b'101 Switching Protocols' in stream.headers
                assert json.loads(stream.frame()[1])['instruments']['live_instrument']['name'] == 'new'
                stream.send(9, b'nginx'); assert stream.frame() == (10, b'nginx')
                assert request('/api/instruments/control/stop', b'')[0] == 200
                deadline = time.monotonic() + 5
                while True:
                    value = json.loads(stream.frame()[1])
                    if value['operation']['phase'] == 'succeeded' and value['instruments']['live_instrument'] is None: break
                    assert time.monotonic() < deadline
                assert request('/api/instruments/control/start', b'')[0] == 200
            finally: stream.close()
            passed('WebSocket status updates and ping through real nginx Unix-socket proxy')
            metadata = {'format': 1, 'board': 'alpha250', 'architecture': 'armhf', 'sdk_version': '1.0', 'min_runtime_api': 1}
            incompatible = package('wrong-board', extra={'serverd': Path('/bin/sleep').read_bytes(), 'instrument.json': json.dumps(metadata).encode()})
            assert upload('wrong-board', incompatible)[0] == 200
            active_pid = ctl('show', '-p', 'MainPID', '--value', INSTRUMENT).stdout
            status, error = request('/api/instruments/activate/wrong-board', b'')
            assert status == 422 and error['code'] == 'board_mismatch'
            assert ctl('show', '-p', 'MainPID', '--value', INSTRUMENT).stdout == active_pid
            passed('board compatibility rejection leaves private instrument running')
            assert upload('broken', package('broken', fail=True))[0] == 200
            status, error = request('/api/instruments/activate/broken', b'')
            assert status == 500 and error['rollback'] == 'restored' and error['code'] == 'start_failed'
            passed('structured readiness failure and successful rollback')
            status, diagnostics = request('/api/system/diagnostics')
            assert status == 200 and diagnostics['logs'] and diagnostics['health']['memory']
            assert len(json.dumps(diagnostics)) < 1024 * 1024
            passed('bounded diagnostic export includes real journal and health')
            if '--hardening' in sys.argv:
                status, operation = request('/api/instruments/activate/new', b'')
                assert status == 200 and operation['phase'] == 'succeeded'
                subprocess.run(['journalctl', '--sync'], check=True, timeout=5)
                current_logs = request('/api/logs/koheron/instrument/incr')[1]
                assert any('native-instrument-new' in entry['msg'] for entry in current_logs['entries'])
                assert all(entry['ts'] >= operation['started_us'] for entry in current_logs['entries'])
                passed('POST activation log bookmark includes first new startup message')
                boot_default = (store / 'old.zip').read_bytes()
                assert upload('old', package('old', extra={'../escape': b'unsafe'}))[0] == 422
                assert upload('old', incompatible)[0] == 422
                assert (store / 'old.zip').read_bytes() == boot_default
                assert (store / 'default').read_text().strip() == 'old.zip'
                assert upload('old', package('old'))[0] == 200
                assert (store / 'default').read_text().strip() == 'old.zip'
                passed('boot-default replacement validates compatibility before committing and preserves valid updates')
                active_pid = ctl('show', '-p', 'MainPID', '--value', INSTRUMENT).stdout
                for path, body in (('run/new', None), ('delete/new', None),
                        ('control/stop', b''), ('activate/old', b''), ('default/new', b''), ('upload', b'ignored')):
                    assert request('/api/instruments/' + path, body, {'Origin': 'http://foreign'})[0] == 403
                assert request('/api/instruments/run/old', headers={'Sec-Fetch-Mode': 'navigate'})[0] == 403
                assert request('/api/instruments/run/old', headers={'Sec-Fetch-Site': 'cross-site'})[0] == 403
                assert ctl('show', '-p', 'MainPID', '--value', INSTRUMENT).stdout == active_pid
                assert (store / 'new.zip').exists() and not list(store.glob('.upload-*'))
                passed('foreign browser mutations and navigation rejected through nginx without side effects')
                for path in ('run/old', 'delete/new'):
                    assert request('/api/instruments/' + path, method='HEAD')[0] == 405
                assert ctl('show', '-p', 'MainPID', '--value', INSTRUMENT).stdout == active_pid
                assert (store / 'new.zip').exists()
                passed('HEAD cannot activate or delete instruments')
                origin = 'http://127.0.0.1:18087'
                assert request('/api/instruments/control/start', b'', {'Origin': origin, 'Referer': origin + '/koheron/',
                    'Sec-Fetch-Site': 'same-origin'})[0] == 200
                stream = WebSocket(18087, f'Origin: {origin}\r\n', '127.0.0.1:18087')
                try: assert b'101 Switching Protocols' in stream.headers; stream.frame()
                finally: stream.close()
                passed('same-origin commands and WebSocket preserve proxy Host port')
                query = urllib.request.Request(origin + '/api/instruments/run/%3Csvg%20onload%3Dalert(1)%3E')
                try: response = urllib.request.urlopen(query, timeout=5)
                except urllib.error.HTTPError as error: response = error
                with response:
                    assert response.headers.get_content_type() == 'text/plain'
                    assert response.headers['X-Content-Type-Options'] == 'nosniff'
                    assert b'<svg' in response.read()
                passed('reflected legacy errors use plain text and nosniff')
                compatible = {**metadata, 'board': 'red-pitaya'}
                malformed = package('malformed', extra={'serverd': Path('/bin/sleep').read_bytes(),
                    'instrument.json': json.dumps(compatible).encode() + b' trailing'})
                assert upload('malformed', malformed)[0] == 200
                assert request('/api/instruments/preflight/malformed')[1]['code'] == 'invalid_archive'
                passed('malformed compatibility JSON is rejected on ARMhf')
                idle = WebSocket(18087); healthy = WebSocket(18087)
                try:
                    started = time.monotonic(); ping_seen = False
                    while time.monotonic() - started < 17:
                        opcode, payload = healthy.frame()
                        if opcode == 9:
                            ping_seen = True; healthy.send(10, payload); idle.send(10, b'wrong')
                        assert opcode != 8
                        if ping_seen and time.monotonic() - started >= 16: break
                    assert ping_seen
                    idle.socket.settimeout(1)
                    try:
                        while True: idle.frame()
                    except EOFError: pass
                    assert request('/api/system/status')[0] == 200
                finally: idle.close(); healthy.close()
                passed('nginx WebSocket heartbeat retains responsive peer and expires unmatched pong')
        else:
            subprocess.run([str(ROOT / 'koheron-server-init'), '/run/koheron-server.sock'],
                env={**os.environ, 'LD_LIBRARY_PATH': str(ROOT / 'lib')}, check=True, timeout=5)
            passed('native LED RPC against the production FFT server')
        data = {'checks': checks, 'activation_ms': activation_ms, 'journal_sample': logs,
            'production_pids_before': before, 'production_api_hash': digest,
            'api_sha256': hashlib.sha256((ROOT / 'koheron-api').read_bytes()).hexdigest(),
            'ui_sha256': hashlib.sha256((ROOT / 'www/instruments.js').read_bytes()).hexdigest() if (ROOT / 'www/instruments.js').exists() else None}
        if '--preview' in sys.argv:
            print('PREVIEW_READY', flush=True)
            deadline = time.monotonic() + 180
            while not (ROOT / 'preview-done').exists() and time.monotonic() < deadline:
                time.sleep(.2)
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
