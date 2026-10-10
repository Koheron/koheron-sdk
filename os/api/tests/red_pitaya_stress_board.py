"""Private board setup/telemetry for red_pitaya_stress.py; never edits production units.

Stage binaries, lib/, koheron-api.{service,socket}, nginx.conf and nginx-server.conf
in /tmp/koheron-native-stress before setup. Python is only the test harness.
"""
import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import shutil
import signal
import stat
import subprocess
import time
import zipfile

ROOT = Path('/tmp/koheron-native-stress')
API = 'koheron-stress-api'
NGINX = 'koheron-stress-nginx'
INSTRUMENT = 'koheron-stress-instrument.service'
LED = 'koheron-stress-led.service'
UNITS = [API + '.service', API + '.socket', NGINX + '.service', INSTRUMENT, LED]


def ctl(*args, check=True):
    return subprocess.run(['systemctl', *args], text=True, capture_output=True,
                          check=check, timeout=40).stdout.strip()


def production():
    return {'pids': {unit: ctl('show', '-p', 'MainPID', '--value', unit)
                     for unit in ['koheron-server', 'uwsgi', 'nginx']},
            'api_sha256': hashlib.sha256(Path('/usr/local/api/app/__init__.py').read_bytes()).hexdigest()}


def package(name, fail=False, unsafe=False):
    script = ('#!/bin/sh\necho stress-instrument-' + name + '\n' +
              ('exit 23\n' if fail else 'exec /usr/bin/systemd-notify --ready --exec ";" -- /bin/sleep infinity\n')).encode()
    members = {'serverd': script, 'version': (name + '-v1').encode(), 'pl.dtbo': b'fixture',
               'fixture.bit.bin': b'fixture', 'drivers.json': b'[{"class":"Common","id":2,"functions":[]}]',
               'payload': bytes(range(256)) * 8192}
    if unsafe: members['../escape'] = b'unsafe'
    result = io.BytesIO()
    with zipfile.ZipFile(result, 'w') as archive:
        for name, data in members.items():
            info = zipfile.ZipInfo(name)
            info.external_attr = (stat.S_IFREG | (0o755 if name == 'serverd' else 0o644)) << 16
            archive.writestr(info, data)
    return result.getvalue()


def setup():
    assert not (ROOT / 'before.json').exists(), 'An earlier stress run is still staged'
    for unit in UNITS:
        assert not (Path('/run/systemd/system') / unit).exists(), unit
    before = production()
    before.update(boot_id=Path('/proc/sys/kernel/random/boot_id').read_text().strip(),
                  started_monotonic_s=time.monotonic(),
                  uname=subprocess.check_output(['uname', '-a'], text=True).strip(),
                  binary_hashes={name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest()
                                 for name in ['koheron-api', 'koheron-install', 'koheron-server-init']})
    (ROOT / 'before.json').write_text(json.dumps(before, indent=2) + '\n')
    store, live = ROOT / 'store', ROOT / 'live'
    store.mkdir(); live.mkdir()
    for name in ['a', 'b', 'broken', 'badpath']:
        (store / (name + '.zip')).write_bytes(package(name, name == 'broken', name == 'badpath'))
    (store / 'default').write_text('a.zip\n')
    with zipfile.ZipFile(store / 'a.zip') as archive: archive.extractall(live)
    (live / 'serverd').chmod(0o755); (live / '.instrument-name').write_text('a\n')
    units = {
        INSTRUMENT: f'''[Unit]
Description=Private stress fixture (no FPGA access)
StartLimitIntervalSec=0
[Service]
Type=notify
ExecStart={live}/serverd
TimeoutStartSec=5s
TimeoutStopSec=5s
''',
        LED: '[Unit]\nStartLimitIntervalSec=0\n[Service]\nType=oneshot\nExecStart=/bin/true\n',
        NGINX + '.service': f'''[Unit]
Description=Private nginx stress frontend
[Service]
Type=simple
ExecStartPre=/usr/sbin/nginx -t -p {ROOT} -c {ROOT}/nginx-private.conf
ExecStart=/usr/sbin/nginx -p {ROOT} -c {ROOT}/nginx-private.conf -g "daemon off;"
KillSignal=SIGQUIT
TimeoutStopSec=10s
'''}
    service = (ROOT / 'koheron-api.service').read_text()
    service = service.replace('[Unit]', '[Unit]\nStartLimitIntervalSec=0')
    service = service.replace('Sockets=koheron-api.socket', f'Sockets={API}.socket')
    service = service.replace('ExecStart=/usr/local/api/koheron-api',
        f'Environment=LD_LIBRARY_PATH={ROOT}/lib\nExecStart={ROOT}/koheron-api --instruments {store} --live {live} --unit {INSTRUMENT} --led-unit {LED}')
    units[API + '.service'] = service
    units[API + '.socket'] = (ROOT / 'koheron-api.socket').read_text().replace('/run/koheron-api/app.sock', str(ROOT / 'http.sock'))
    config = (ROOT / 'nginx.conf').read_text().replace('/run/nginx.pid', str(ROOT / 'nginx.pid'))
    config = config.replace('/run/nginx/', str(ROOT / 'nginx-tmp') + '/')
    config = config.replace('include             /etc/nginx/conf.d/*.conf;', '')
    config = config.replace('/etc/nginx/sites-enabled/*', str(ROOT / 'server.conf'))
    (ROOT / 'nginx-tmp').mkdir()
    (ROOT / 'nginx-private.conf').write_text(config)
    (ROOT / 'server.conf').write_text((ROOT / 'nginx-server.conf').read_text()
        .replace('listen                      80;', 'listen 192.168.1.85:18089;')
        .replace('/run/koheron-api/app.sock', str(ROOT / 'http.sock')))
    for name, text in units.items(): (Path('/run/systemd/system') / name).write_text(text)
    ctl('daemon-reload'); ctl('start', INSTRUMENT, API + '.socket', API + '.service', NGINX)
    (ROOT / 'phase').write_text('initial-idle')
    with (ROOT / 'monitor.log').open('w') as log:
        child = subprocess.Popen(['/usr/bin/python3', str(ROOT / 'red_pitaya_stress_board.py'), 'monitor'],
                                 stdin=subprocess.DEVNULL, stdout=log, stderr=log, start_new_session=True)
    (ROOT / 'monitor.pid').write_text(str(child.pid))
    print(json.dumps(before), flush=True)


def process(pid):
    root = Path('/proc') / str(pid)
    status = {}
    for line in (root / 'smaps_rollup').read_text().splitlines():
        if line.startswith(('Pss:', 'Rss:', 'Private_Dirty:')): status[line.split(':')[0] + '_kib'] = int(line.split()[1])
    fields = (root / 'stat').read_text().rsplit(')', 1)[1].split()
    status.update(pid=pid, cpu_ticks=int(fields[11]) + int(fields[12]), threads=int(fields[17]),
                  fd_count=len(list((root / 'fd').iterdir())))
    return status


def monitor():
    with (ROOT / 'telemetry.jsonl').open('w', buffering=1) as output:
        while not (ROOT / 'monitor.stop').exists():
            sample = {'monotonic_s': time.monotonic(), 'phase': (ROOT / 'phase').read_text().strip()}
            try:
                pid = int(ctl('show', '-p', 'MainPID', '--value', API))
                sample['api'] = process(pid) if pid else None
                sample['nginx'] = []
                master = int((ROOT / 'nginx.pid').read_text())
                children = []
                for candidate in Path('/proc').iterdir():
                    if not candidate.name.isdecimal(): continue
                    try:
                        fields = (candidate / 'stat').read_text().rsplit(')', 1)[1].split()
                        if int(fields[1]) == master: children.append(int(candidate.name))
                    except FileNotFoundError: pass
                for pid in [master, *children]: sample['nginx'].append(process(pid))
            except (OSError, ValueError, subprocess.SubprocessError) as error: sample['sample_error'] = str(error)
            sample['mem_available_kib'] = next(int(line.split()[1]) for line in Path('/proc/meminfo').read_text().splitlines() if line.startswith('MemAvailable:'))
            sample['tmp_free_bytes'] = shutil.disk_usage('/tmp').free
            output.write(json.dumps(sample) + '\n'); time.sleep(1)


def cleanup():
    (ROOT / 'monitor.stop').touch()
    time.sleep(1.2)
    after = production()
    leftovers = [str(p.relative_to(ROOT)) for pattern in ['store/.upload-*', '.instrument-*', 'nginx-tmp/client-body/*'] for p in ROOT.glob(pattern)]
    metadata = {'before': json.loads((ROOT / 'before.json').read_text()), 'after': after,
                'leftover_temporary_files': leftovers,
                'api_service': ctl('show', API, '-p', 'NRestarts', '-p', 'Result', '-p', 'ExecMainStatus'),
                'journal': subprocess.check_output(['journalctl', '-b', '--no-pager', '-o', 'short-monotonic',
                                                    '-u', API, '-u', INSTRUMENT], text=True),
                'nginx_journal': subprocess.check_output(['journalctl', '-b', '--no-pager', '-o', 'short-monotonic', '-u', NGINX], text=True),
                'monitor_log': (ROOT / 'monitor.log').read_text(),
                'kernel_log': subprocess.check_output(['dmesg'], text=True)}
    for unit in UNITS: ctl('stop', unit, check=False)
    for unit in UNITS:
        ctl('reset-failed', unit, check=False)
        (Path('/run/systemd/system') / unit).unlink(missing_ok=True)
    ctl('daemon-reload')
    metadata['temporary_units_removed'] = all(not (Path('/run/systemd/system') / u).exists() for u in UNITS)
    metadata['production_unchanged'] = all(metadata['before'][key] == after[key] for key in ['pids', 'api_sha256'])
    (ROOT / 'cleanup.json').write_text(json.dumps(metadata, indent=2) + '\n')
    assert metadata['production_unchanged'] and metadata['temporary_units_removed']
    print(json.dumps({k: metadata[k] for k in ['after', 'leftover_temporary_files', 'production_unchanged', 'temporary_units_removed']}))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=['setup', 'monitor', 'cleanup'])
    args = parser.parse_args()
    globals()[args.action]()
