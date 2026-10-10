"""Opt-in benchmark on an existing Python-based V1 board, using isolated ports.

Stage the three native binaries, their missing shared libraries, and the V1
Python API as python-api.py/service_status.py in --directory. Production files
and services are read only. The Python harness is not part of the board image.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import signal
import statistics
import subprocess
import time
import urllib.error
import urllib.request


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--directory', default='/tmp/native-management')
    parser.add_argument('--port', type=int, default=18085)
    args = parser.parse_args()
    root = Path(args.directory)
    app = root / 'python/app'
    app.mkdir(parents=True, exist_ok=True)
    (app / '__init__.py').write_bytes((root / 'python-api.py').read_bytes())
    (app / 'service_status.py').write_bytes((root / 'service_status.py').read_bytes())
    (app.parent / 'wsgi.py').write_text('from app import app as application\n')
    production = Path('/usr/local/api/app/__init__.py')
    production_hash = hashlib.sha256(production.read_bytes()).hexdigest()
    def service_pids():
        return {unit: subprocess.check_output(['systemctl', 'show', '-p', 'MainPID', '--value', unit],
            text=True).strip() for unit in ('koheron-server', 'uwsgi', 'nginx')}
    before = service_pids()
    def get(path):
        try: response = urllib.request.urlopen(f'http://127.0.0.1:{args.port}' + path, timeout=2)
        except urllib.error.HTTPError as error: response = error
        with response:
            body = response.read()
            if response.headers.get_content_type() == 'application/json': body = json.loads(body)
            else: body = body.decode()
            return {'status': response.status, 'body': body}
    def descendants(pid):
        result = [pid]
        for parent in result:
            for entry in Path('/proc').iterdir():
                if not entry.name.isdecimal() or int(entry.name) in result: continue
                try:
                    stat = (entry / 'stat').read_text().rsplit(')', 1)[1].split()
                    if int(stat[1]) == parent: result.append(int(entry.name))
                except FileNotFoundError: pass
        return result
    paths = ['/api/instruments', '/api/instruments/details', '/api/system/build',
        '/api/system/manifest', '/api/system/manifest/raw', '/api/system/release',
        '/api/system/release/raw', '/api/logs/koheron?lines=10', '/api/logs/koheron/bookmark',
        '/api/logs/koheron/incr', '/api/logs/koheron/instrument/bookmark',
        '/api/logs/koheron/instrument/incr', '/api/instruments/commands/fft']
    results = []
    for iteration in range(3):
        for variant in (('python', 'native') if iteration % 2 == 0 else ('native', 'python')):
            native = variant == 'native'
            command = ([str(root / 'koheron-api'), '--port', str(args.port)] if native else
                ['uwsgi', '--plugin', 'python3', '--chdir', str(app.parent), '--wsgi-file', str(app.parent / 'wsgi.py'),
                 '--http-socket', f'127.0.0.1:{args.port}', '--master',
                 '--processes', '1', '--threads', '1', '--need-app', '--die-on-term', '--close-on-exec'])
            environment = {**os.environ, 'LD_LIBRARY_PATH': str(root / 'lib')}
            environment.pop('INVOCATION_ID', None)
            with (root / f'{variant}-{iteration}.log').open('wb') as output:
                started = time.monotonic()
                process = subprocess.Popen(command, stdout=output, stderr=subprocess.STDOUT, env=environment)
                try:
                    while True:
                        if process.poll() is not None: raise RuntimeError('Daemon exited; inspect log')
                        try: get('/api/instruments/details'); break
                        except OSError:
                            if time.monotonic() - started > 15: raise RuntimeError('Readiness timeout')
                            time.sleep(0.005)
                    ready = (time.monotonic() - started) * 1000
                    for _ in range(20): get('/api/instruments/details')
                    latencies = []
                    for _ in range(100):
                        start = time.monotonic(); get('/api/instruments/details')
                        latencies.append((time.monotonic() - start) * 1000)
                    pids = descendants(process.pid)
                    memory = []
                    for _ in range(5):
                        values = {}
                        for pid in pids:
                            rows = Path(f'/proc/{pid}/smaps_rollup').read_text().splitlines()
                            values[str(pid)] = {row.split(':')[0]: int(row.split()[1])
                                for row in rows if row.startswith(('Pss:', 'Rss:', 'Private_Dirty:'))}
                        memory.append(values)
                    result = {'variant': variant, 'iteration': iteration, 'ready_ms': ready,
                        'status_latency_ms': latencies, 'memory_kib': memory,
                        'pss_kib': statistics.median(sum(item['Pss'] for item in sample.values()) for sample in memory),
                        'responses': {path: get(path) for path in paths}}
                    results.append(result)
                    print(json.dumps({key: result[key] for key in ('variant', 'iteration', 'ready_ms', 'pss_kib')}), flush=True)
                finally:
                    process.send_signal(signal.SIGTERM if native else signal.SIGINT)
                    try: process.wait(timeout=8)
                    except subprocess.TimeoutExpired:
                        for pid in descendants(process.pid):
                            try: os.kill(pid, signal.SIGKILL)
                            except ProcessLookupError: pass
                        process.wait(timeout=3)
    summary = {variant: {'ready_ms_median': statistics.median(r['ready_ms'] for r in results if r['variant'] == variant),
        'pss_kib_median': statistics.median(r['pss_kib'] for r in results if r['variant'] == variant),
        'status_latency_ms_median': statistics.median(ms for r in results if r['variant'] == variant for ms in r['status_latency_ms'])}
        for variant in ('python', 'native')}
    data = {'boot_id': Path('/proc/sys/kernel/random/boot_id').read_text().strip(),
        'uname': subprocess.check_output(['uname', '-a'], text=True).strip(),
        'production_api_hash': production_hash,
        'baseline_source_hash': hashlib.sha256((root / 'python-api.py').read_bytes()).hexdigest(),
        'binary_hashes': {name: hashlib.sha256((root / name).read_bytes()).hexdigest()
            for name in ('koheron-api', 'koheron-install', 'koheron-server-init')},
        'production_api_unchanged': production_hash == hashlib.sha256(production.read_bytes()).hexdigest(),
        'production_pids_before': before, 'production_pids_after': service_pids(),
        'results': results, 'summary': summary}
    (root / 'benchmark.json').write_text(json.dumps(data, indent=2) + '\n')
    assert data['production_api_unchanged'] and before == data['production_pids_after']
    print(json.dumps(summary), flush=True)


if __name__ == '__main__': main()
