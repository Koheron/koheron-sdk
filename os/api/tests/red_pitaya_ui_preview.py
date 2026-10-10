"""Short, opt-in UI preview on an existing laboratory board.

Stage the same private binaries, libraries, configs and www/ as the integration
harness under /tmp/native-management. Run with --preview; touch preview-done to
finish early. It stages four dummy instruments, never programs the FPGA, and
removes private services without changing production units.
"""
import hashlib
import io
import json
from pathlib import Path
import signal
import subprocess
import sys
import time
import zipfile

from red_pitaya_integration import ROOT, API, INSTRUMENT, LED, ctl, pids, package, request, upload


def main():
    if '--preview' not in sys.argv:
        raise SystemExit('Use --preview to expose the private UI listener.')
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
        assert upload('scope', package('scope'))[0] == 200
        assert upload('wide-spectrum-analysis', package('1.0.0-development+red-pitaya'))[0] == 200
        metadata = {'format': 1, 'board': 'alpha250', 'architecture': 'armhf', 'sdk_version': '1.0', 'min_runtime_api': 1}
        assert upload('wrong-board', package('wrong-board', extra={'serverd': Path('/bin/sleep').read_bytes(), 'instrument.json': json.dumps(metadata).encode()}))[0] == 200
        passed('private UI fixtures staged without changing production instruments')
        data = {'scope': 'Targeted UI preview, not a repeat of the native integration suite', 'checks': checks,
            'activation_ms': activation_ms,
            'production_pids_before': before, 'production_api_hash': digest,
            'api_sha256': hashlib.sha256((ROOT / 'koheron-api').read_bytes()).hexdigest(),
            'ui_sha256': hashlib.sha256((ROOT / 'www/instruments.js').read_bytes()).hexdigest()}
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
