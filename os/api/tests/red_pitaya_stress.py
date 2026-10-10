"""Opt-in LAN stress client for a staged private Red Pitaya runtime.

Run red_pitaya_stress_board.py setup first. Host Python generates load; board
Python only sets up private fixtures and samples telemetry. No production
instrument is stopped and fixture installs never access the FPGA.
"""
import argparse
from collections import Counter
import concurrent.futures
import http.client
import json
import math
from pathlib import Path
import shlex
import socket
import subprocess
import threading
import time

ROOT = '/tmp/koheron-native-stress'
PATHS = ['/api/instruments/details'] * 5 + ['/api/instruments', '/api/system/build',
    '/api/system/manifest', '/api/system/release', '/api/instruments/commands/a',
    '/api/logs/koheron?lines=100', '/api/logs/koheron/bookmark',
    '/api/logs/koheron/instrument/bookmark', '/api/logs/koheron/instrument/incr']
BINS = [1, 2, 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, float('inf')]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--host', default='192.168.1.85')
    parser.add_argument('--port', type=int, default=18089)
    parser.add_argument('--ssh-command', default='ssh root@192.168.1.85')
    parser.add_argument('--output', default='tmp/native-stress/load.json')
    parser.add_argument('--ramp-seconds', type=int, default=30)
    parser.add_argument('--soak-seconds', type=int, default=300)
    args = parser.parse_args()
    results, actions = [], []
    destination = Path(args.output); destination.parent.mkdir(parents=True, exist_ok=True)

    def ssh(command):
        return subprocess.run([*shlex.split(args.ssh_command), command], check=True,
                              capture_output=True, text=True, timeout=70).stdout.strip()

    def phase(name):
        ssh('printf %s ' + shlex.quote(name) + ' > ' + ROOT + '/phase')
        print(json.dumps({'phase': name, 'time': time.time()}), flush=True)

    def connection(): return http.client.HTTPConnection(args.host, args.port, timeout=10)

    def validate(path, body, allow_null):
        data = json.loads(body)
        if path in ['/api/instruments/details', '/api/instruments']:
            assert len(data['instruments']) >= 4
            live = data['live_instrument']
            if live is None:
                assert allow_null, 'Live identity unexpectedly null outside activation'
            elif path.endswith('/details'):
                assert live['name'] in ['a', 'b']
                assert live['version'] == live['name'] + '-v1', 'Mixed installation snapshot'
                assert live['is_default'] == (live['name'] == 'a')
            else: assert live in ['a', 'b']
        elif '/logs/' in path:
            assert 'cursor' in data
            if not path.endswith('/bookmark'): assert isinstance(data['entries'], list)
        elif '/commands/' in path: assert data[0]['class'] == 'Common'
        else: assert isinstance(data, dict)

    def save():
        destination.write_text(json.dumps({'host': args.host, 'port': args.port,
            'ramp_seconds': args.ramp_seconds, 'soak_seconds': args.soak_seconds,
            'profile': PATHS, 'results': results, 'actions': actions}, indent=2) + '\n')

    def load(name, clients, seconds, allow_null=False, disruptions=False):
        phase(name)
        barrier = threading.Barrier(clients)
        def worker(index):
            conn = connection(); codes = Counter(); errors = Counter(); paths = Counter()
            latencies, examples, nulls = [], [], 0
            barrier.wait(); started = time.monotonic(); count = 0
            try:
                while time.monotonic() - started < seconds:
                    path = PATHS[(index + count) % len(PATHS)]; count += 1
                    begin = time.monotonic(); status = None
                    try:
                        conn.request('GET', path)
                        response = conn.getresponse(); status = response.status; body = response.read()
                        codes[str(status)] += 1; paths[path] += 1
                        if status != 200: raise RuntimeError('HTTP ' + str(status))
                        validate(path, body, allow_null)
                        if path in ['/api/instruments/details', '/api/instruments'] and json.loads(body)['live_instrument'] is None: nulls += 1
                        latencies.append((time.monotonic() - begin) * 1000)
                    except Exception as error:
                        category = 'HTTP ' + str(status) if status and status != 200 else type(error).__name__
                        errors[category] += 1
                        if len(examples) < 3: examples.append({'path': path, 'error': str(error), 'status': status})
                        conn.close(); conn = connection(); time.sleep(.01)
            finally: conn.close()
            return {'requests': count, 'codes': codes, 'errors': errors, 'paths': paths,
                    'latencies': latencies, 'examples': examples, 'nulls': nulls,
                    'elapsed_s': time.monotonic() - started}
        started = time.monotonic()
        with concurrent.futures.ThreadPoolExecutor(max_workers=clients) as pool:
            rows = list(pool.map(worker, range(clients)))
        elapsed = time.monotonic() - started
        codes, errors, paths = Counter(), Counter(), Counter()
        for row in rows: codes.update(row['codes']); errors.update(row['errors']); paths.update(row['paths'])
        times = sorted(ms for row in rows for ms in row['latencies'])
        def percentile(p): return times[min(len(times) - 1, math.ceil(p * len(times)) - 1)] if times else None
        histogram = Counter(str(next(limit for limit in BINS if ms <= limit)) for ms in times)
        expected = {'HTTP 502', 'HTTP 504', 'ConnectionResetError', 'RemoteDisconnected', 'BrokenPipeError', 'TimeoutError', 'ConnectionRefusedError'} if disruptions else set()
        result = {'phase': name, 'clients': clients, 'duration_s': elapsed,
                  'requests': sum(r['requests'] for r in rows), 'successful_requests': len(times),
                  'successful_requests_per_s': len(times) / elapsed, 'status_counts': dict(codes),
                  'errors': dict(errors), 'unexpected_errors': {k: v for k, v in errors.items() if k not in expected},
                  'latency_ms': {'p50': percentile(.5), 'p95': percentile(.95), 'p99': percentile(.99), 'max': times[-1] if times else None},
                  'latency_histogram_upper_ms': dict(histogram), 'path_counts': dict(paths),
                  'error_examples': [e for r in rows for e in r['examples']][:25],
                  'null_live_snapshots_during_activation': sum(r['nulls'] for r in rows)}
        results.append(result); save(); print(json.dumps(result), flush=True)
        return result

    def request(path, data=None, headers=None, expected=200):
        conn = connection()
        try:
            conn.request('POST' if data is not None else 'GET', path, data, headers or {})
            response = conn.getresponse(); body = response.read()
            assert response.status == expected, (path, response.status, body[:300])
            return body
        finally: conn.close()

    def idle(name, seconds=15):
        phase(name); time.sleep(seconds)

    def recover():
        started = time.monotonic()
        while time.monotonic() - started < 15:
            try:
                validate('/api/instruments/details', request('/api/instruments/details'), False)
                return (time.monotonic() - started) * 1000
            except (OSError, http.client.HTTPException, AssertionError): time.sleep(.01)
        raise RuntimeError('API did not recover within 15 seconds')

    try:
        idle('cold-idle')
        for clients in [1, 8, 32, 64]: load('ramp-' + str(clients), clients, args.ramp_seconds)
        idle('warm-idle-before-soak')
        load('soak-32', 32, args.soak_seconds)
        idle('warm-idle-after-soak')
        with concurrent.futures.ThreadPoolExecutor(max_workers=1) as pool:
            readers = pool.submit(load, 'mixed-mutations-16', 16, 240, True)
            time.sleep(2)
            archive = request('/api/instruments/commands/a')  # exercise a read during the first mutations
            del archive
            package = ssh('base64 -w0 ' + ROOT + '/store/b.zip')
            import base64
            payload = base64.b64decode(package)
            boundary = 'native-stress-boundary-72913'
            body = (f'--{boundary}\r\nContent-Disposition: form-data; name="uploaded.zip"; filename="uploaded.zip"\r\n'
                    'Content-Type: application/zip\r\n\r\n').encode() + payload + f'\r\n--{boundary}--\r\n'.encode()
            for index in range(30):
                assert not readers.done(), 'Mutation phase outlasted its concurrent readers'
                target = 'b' if index % 2 == 0 else 'a'
                request('/api/instruments/run/' + target)
                details = json.loads(request('/api/instruments/details'))
                assert details['live_instrument']['name'] == target
                if index % 3 == 0:
                    request('/api/instruments/run/broken', expected=500)
                    assert json.loads(request('/api/instruments/details'))['live_instrument']['name'] == target
                    request('/api/instruments/run/badpath', expected=500)
                    assert json.loads(request('/api/instruments/details'))['live_instrument']['name'] == target
                request('/api/instruments/upload', body, {'Content-Type': 'multipart/form-data; boundary=' + boundary})
                request('/api/instruments/delete/uploaded')
                actions.append({'iteration': index, 'target': target, 'upload_bytes': len(body), 'rollback_and_unsafe_extraction': index % 3 == 0})
                print(json.dumps(actions[-1]), flush=True)
            readers.result()
        idle('after-mutations')
        # Direct Unix-socket interruptions reach the API, bypassing nginx body buffering.
        phase('aborted-uploads')
        aborted = ssh("python3 - <<'PY'\nimport socket,time,json\nfrom pathlib import Path\nroot=Path('" + ROOT + "')\nfor i in range(500):\n s=socket.socket(socket.AF_UNIX,socket.SOCK_STREAM);s.settimeout(5);s.connect(str(root/'http.sock'))\n s.sendall(b'POST /api/instruments/upload HTTP/1.1\\r\\nHost: localhost\\r\\nContent-Length: 99999\\r\\nContent-Type: multipart/form-data; boundary=boundary\\r\\n\\r\\n--boundary\\r\\nContent-Disposition: form-data; name=\"interrupted.zip\"; filename=\"interrupted.zip\"\\r\\n\\r\\npartial upload')\n s.close()\ntime.sleep(2)\nremaining=list((root/'store').glob('.upload-*'))\nassert not remaining,remaining\nprint(json.dumps({'aborted_uploads':500,'leftovers':[]}))\nPY")
        actions.append(json.loads(aborted)); recover()
        idle('after-aborted-uploads')
        with concurrent.futures.ThreadPoolExecutor(max_workers=1) as pool:
            readers = pool.submit(load, 'restart-under-load-16', 16, 240, False, True)
            time.sleep(2)
            for index in range(50):
                assert not readers.done(), 'Restart phase outlasted its concurrent readers'
                begin = time.monotonic(); ssh('systemctl restart koheron-stress-api.service')
                ready = recover()
                actions.append({'restart': index, 'total_restart_to_http_ms': (time.monotonic() - begin) * 1000,
                                'http_after_systemctl_ms': ready})
                print(json.dumps(actions[-1]), flush=True)
            for index in range(3):
                begin = time.monotonic(); ssh('systemctl kill --kill-whom=main --signal=SIGKILL koheron-stress-api.service')
                ready = recover()
                actions.append({'forced_kill': index, 'automatic_recovery_to_http_ms': (time.monotonic() - begin) * 1000,
                                'http_after_kill_command_ms': ready})
                print(json.dumps(actions[-1]), flush=True)
            readers.result()
        idle('final-idle'); save()
        assert not any(r['unexpected_errors'] for r in results), 'Unexpected load errors; inspect raw result'
    finally:
        print(ssh('python3 ' + ROOT + '/red_pitaya_stress_board.py cleanup'), flush=True)


if __name__ == '__main__': main()
