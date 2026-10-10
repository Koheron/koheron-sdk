"""Opt-in, disruptive paired reboot test on an existing Python-based V1 board.

Stage this script, boot_http_probe and variants/{python,native}/ below
/var/lib/koheron-native-boot-evidence. Variant trees mirror absolute paths;
Python API/LED files and native binaries/libraries live under bundle/ instead.
prepare saves every touched configuration and installs a three-minute recovery
timer. select configures the NEXT boot; collect records the completed boot;
restore reinstates and verifies the original files/links, then restarts API,
nginx and the LED helper. It never changes the kernel, bootloader or SD layout.
The board retains its installed Python packages in both variants.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import stat
import subprocess
import tarfile

ROOT = Path('/var/lib/koheron-native-boot-evidence')
UNITS = ['uwsgi.service', 'uwsgi.socket', 'koheron-api.service', 'koheron-api.socket',
         'nginx.service', 'koheron-server.service', 'unzip-default-instrument.service',
         'koheron-server-init.service', 'koheron-boot-evidence-probe.service',
         'koheron-boot-evidence-rollback.service', 'koheron-boot-evidence-rollback.timer']
PATHS = [Path('/etc/systemd/system') / unit for unit in UNITS]
PATHS += [Path('/etc/systemd/system') / target / unit for target in
          ('basic.target.wants', 'multi-user.target.wants', 'sockets.target.wants', 'timers.target.wants')
          for unit in UNITS]
PATHS += [Path('/etc/nginx/nginx.conf'), Path('/etc/nginx/sites-available/koheron.conf')]


def command(*args, check=True):
    return subprocess.run(args, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                          check=check).stdout.strip()


def signature(path):
    if path.is_symlink():
        return {'link': os.readlink(path)}
    if not path.exists():
        return None
    return {'sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
            'mode': stat.S_IMODE(path.stat().st_mode)}


def prepare():
    if (ROOT / 'original.json').exists():
        raise RuntimeError('Original backup already exists; restore before preparing again')
    snapshot = {str(p): signature(p) for p in PATHS}
    with tarfile.open(ROOT / 'original.tar', 'w') as archive:
        for path in PATHS:
            if snapshot[str(path)] is not None:
                archive.add(path, arcname=str(path).lstrip('/'), recursive=False)
    (ROOT / 'original.json').write_text(json.dumps(snapshot, indent=2) + '\n')
    (ROOT / 'original-system.json').write_text(json.dumps({
        'boot_id': Path('/proc/sys/kernel/random/boot_id').read_text().strip(),
        'uname': command('uname', '-a'),
        'boot_time': command('systemd-analyze', 'time'),
        'production_api_sha256': signature(Path('/usr/local/api/app/__init__.py')),
        'boot_artifact_hashes': command('sha256sum', '/boot/kernel.itb', '/boot/boot.bin'),
        'instrument_archive_hash': command('sha256sum', '/usr/local/instruments/fft.zip'),
    }, indent=2) + '\n')


def select(variant):
    assert (ROOT / 'original.tar').exists()
    for path in PATHS:
        if path.is_symlink() or path.is_file():
            path.unlink()
    for source in (ROOT / 'variants' / variant).rglob('*'):
        if source.is_file():
            destination = Path('/') / source.relative_to(ROOT / 'variants' / variant)
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, destination)
    units = Path('/etc/systemd/system')
    (units / 'koheron-boot-evidence-probe.service').write_text('''[Unit]
Description=Independent nginx API boot readiness measurement
DefaultDependencies=no
After=systemd-remount-fs.service
Before=shutdown.target
Conflicts=shutdown.target
[Service]
Type=simple
ExecStart=/var/lib/koheron-native-boot-evidence/boot_http_probe /run/koheron-boot-http.json
[Install]
WantedBy=multi-user.target
''')
    (units / 'koheron-boot-evidence-rollback.service').write_text('''[Unit]
Description=Restore original board configuration if boot testing loses SSH
[Service]
Type=oneshot
ExecStart=/usr/bin/python3 /var/lib/koheron-native-boot-evidence/red_pitaya_boot_evidence.py restore
''')
    (units / 'koheron-boot-evidence-rollback.timer').write_text('''[Unit]
Description=Boot test automatic recovery after three minutes
[Timer]
OnBootSec=180s
Unit=koheron-boot-evidence-rollback.service
[Install]
WantedBy=timers.target
''')
    command('systemctl', 'daemon-reload')
    api = 'uwsgi' if variant == 'python' else 'koheron-api'
    command('systemctl', 'enable', api + '.service', api + '.socket', 'nginx.service',
            'koheron-server.service', 'unzip-default-instrument.service',
            'koheron-server-init.service', 'koheron-boot-evidence-probe.service',
            'koheron-boot-evidence-rollback.timer')
    command('nginx', '-t')
    (ROOT / 'selected').write_text(variant + '\n')
    command('sync')


def collect(output):
    variant = (ROOT / 'selected').read_text().strip()
    api = 'uwsgi.service' if variant == 'python' else 'koheron-api.service'
    unit_names = [api, 'nginx.service', 'koheron-server.service', 'koheron-server-init.service',
                  'basic.target', 'multi-user.target', 'graphical.target',
                  'systemd-networkd.service', 'systemd-networkd-wait-online.service',
                  'koheron-boot-evidence-probe.service']
    properties = ['ExecMainStartTimestampMonotonic', 'ActiveEnterTimestampMonotonic',
                  'ExecMainExitTimestampMonotonic', 'ActiveState', 'SubState', 'Result', 'MainPID']
    data = {'variant': variant,
            'boot_id': Path('/proc/sys/kernel/random/boot_id').read_text().strip(),
            'uname': command('uname', '-a'),
            'http_probe': json.loads(Path('/run/koheron-boot-http.json').read_text()),
            'systemd_time': command('systemd-analyze', 'time'),
            'manager': command('systemctl', 'show', '-p', 'KernelTimestampMonotonic',
                               '-p', 'UserspaceTimestampMonotonic', '-p', 'FinishTimestampMonotonic'),
            'critical_chain': command('systemd-analyze', 'critical-chain'),
            'blame': command('systemd-analyze', 'blame'),
            'units': {unit: dict(line.split('=', 1) for line in
                       command('systemctl', 'show', unit, *['--property=' + p for p in properties]).splitlines())
                      for unit in unit_names},
            'details': json.loads(command('curl', '-fsS', 'http://127.0.0.1/api/instruments/details')),
            'boot_artifact_hashes': command('sha256sum', '/boot/kernel.itb', '/boot/boot.bin'),
            'instrument_archive_hash': command('sha256sum', '/usr/local/instruments/fft.zip'),
            'production_api_sha256': signature(Path('/usr/local/api/app/__init__.py')),
            'journal': command('journalctl', '-b', '-o', 'short-monotonic', '--no-pager',
                               '-u', api, '-u', 'nginx', '-u', 'koheron-server', '-u', 'koheron-server-init'),
            'configuration': {str(p): signature(p) for p in PATHS},
            'bundle_hashes': {str(p.relative_to(ROOT)): signature(p) for p in (ROOT / 'bundle').rglob('*') if p.is_file()},
    }
    Path(output).write_text(json.dumps(data, indent=2) + '\n')
    print(json.dumps({'variant': variant, 'boot_id': data['boot_id'], 'probe': data['http_probe'],
                      'boot': data['systemd_time']}))


def restore():
    command('systemctl', 'disable', '--now', 'koheron-boot-evidence-rollback.timer', check=False)
    command('systemctl', 'stop', 'koheron-api.service', 'koheron-api.socket', 'uwsgi.service',
            'uwsgi.socket', 'nginx.service', 'koheron-server-init.service', check=False)
    for path in PATHS:
        if path.is_file() or path.is_symlink():
            path.unlink()
    with tarfile.open(ROOT / 'original.tar') as archive:
        archive.extractall('/', filter='fully_trusted')
    expected = json.loads((ROOT / 'original.json').read_text())
    actual = {str(p): signature(p) for p in PATHS}
    assert expected == actual, 'Restored configuration differs from original snapshot'
    command('systemctl', 'daemon-reload')
    command('nginx', '-t')
    command('systemctl', 'start', 'uwsgi.socket', 'uwsgi.service', 'nginx.service',
            'koheron-server-init.service')
    result = {'configuration_restored': True,
              'production_api_sha256': signature(Path('/usr/local/api/app/__init__.py')),
              'api_response': json.loads(command('curl', '-fsS', 'http://127.0.0.1/api/instruments/details')),
              'service_states': command('systemctl', 'is-active', 'uwsgi', 'nginx', 'koheron-server'),
              'boot_artifact_hashes': command('sha256sum', '/boot/kernel.itb', '/boot/boot.bin')}
    (ROOT / 'restored.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=['prepare', 'select', 'collect', 'restore'])
    parser.add_argument('--variant', choices=['python', 'native'])
    parser.add_argument('--output')
    args = parser.parse_args()
    if args.action == 'select' and args.variant is None:
        parser.error('select requires --variant')
    if args.action == 'collect' and args.output is None:
        parser.error('collect requires --output')
    assert os.geteuid() == 0, 'Run on the test board as root'
    if args.action == 'prepare': prepare()
    elif args.action == 'select': select(args.variant)
    elif args.action == 'collect': collect(args.output)
    else: restore()
