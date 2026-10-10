"""Host-side production log verification: GET/WS and TCP connect/close only."""
import argparse
import datetime
import json
from pathlib import Path
import socket
import time
import urllib.parse
import urllib.request

from test_native_management import WebSocket


def verify(host):
    checks, samples = [], []

    def http(path):
        with urllib.request.urlopen('http://' + host + path, timeout=5) as response:
            body = response.read()
        assert len(body) <= 65536
        return json.loads(body)

    def connect(cursor=None, headers='', invocation=None):
        path = '/api/logs/koheron/events'
        query = {}
        if cursor: query['cursor'] = cursor
        if invocation: query['invocation'] = invocation
        if query: path += '?' + urllib.parse.urlencode(query)
        return WebSocket(80, host=host, path=path, address=host, headers=headers)

    def receive(stream):
        while True:
            opcode, payload = stream.frame()
            if opcode == 9:
                stream.send(10, payload); continue
            assert opcode == 1, (opcode, payload)
            assert len(payload) <= 65536
            value = json.loads(payload)
            assert value['type'] == 'logs'
            assert len(value['entries']) <= 200
            assert all(0 <= entry['prio'] <= 7 for entry in value['entries'])
            assert all(isinstance(entry['truncated'], bool) for entry in value['entries'])
            return value

    def client_close():
        with socket.create_connection((host, 36000), timeout=3):
            pass

    stream = connect()
    try:
        assert b'101' in stream.headers
        first = receive(stream)
        tail = http('/api/logs/koheron/tail')
        assert first == tail, 'WS initial history must match bounded HTTP history'
        assert first['entries'] and not first['reset']
        checks.append('port-80 WS initial history matches bounded HTTP, with priority')
        cursor = first['cursor']
        for _ in range(3):
            started = time.monotonic()
            client_close()
            deadline = started + 3
            while True:
                value = receive(stream)
                assert time.monotonic() < deadline
                if any('TCPSocket: Connection closed by client' in entry['msg'] for entry in value['entries']):
                    break
            assert value['cursor'] != cursor
            cursor = value['cursor']
            samples.append(round((time.monotonic() - started) * 1000, 2))
        checks.append('three real instrument TCP disconnects arrive as incremental WS log batches')
    finally:
        stream.close()
    invocation = http('/api/system/status')['health']['instrument_service']['invocation']
    assert len(invocation) == 32 and invocation != '0' * 32
    current = http('/api/logs/koheron/tail?invocation=' + invocation)
    stream = connect(invocation=invocation)
    try:
        assert b'101' in stream.headers
        assert receive(stream) == current
        assert current['entries'] and not current['reset']
        checks.append('current service invocation is exposed in health; filtered WS and HTTP history agree')
    finally:
        stream.close()
    empty = http('/api/logs/koheron/tail?invocation=' + '0' * 32)
    assert empty['entries'] == [] and empty['cursor'] is None and not empty['reset']
    checks.append('an unknown invocation returns no entries and never falls back to other runs')
    client_close()
    deadline = time.monotonic() + 3
    while True:
        expected = http('/api/logs/koheron/tail?cursor=' + urllib.parse.quote(cursor, safe=''))
        if expected['entries']:
            break
        assert time.monotonic() < deadline
        time.sleep(.05)
    assert expected['entries']
    stream = connect(cursor)
    try:
        resumed = receive(stream)
        assert resumed == expected and not resumed['reset']
        checks.append('reconnect replays only entries after the saved cursor')
    finally:
        stream.close()
    stream = connect('missing')
    try:
        recovered = receive(stream)
        assert recovered['reset'] and recovered['entries']
        checks.append('invalid or expired cursor resets to bounded recent history')
    finally:
        stream.close()
    stream = connect(headers='Origin: http://foreign\r\n')
    try:
        assert b'403' in stream.headers
        checks.append('nginx and native API reject foreign browser origins')
    finally:
        stream.close()
    return {
        'recorded_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
        'host': host, 'checks': checks, 'initial_entries': len(first['entries']),
        'replayed_entries': len(resumed['entries']), 'reset_entries': len(recovered['entries']),
        'service_invocation': invocation, 'current_run_entries': len(current['entries']),
        'delivery_ms': samples,
        'measurement': 'Three light TCP connect/close samples; client-close to receipt on the host, including network and journald delay; not a stress test or a boot benchmark.',
        'scope': 'Production GET and read-only WebSockets; four instrument TCP connect/close probes; no lifecycle, settings, upload, FPGA or reboot actions.'
    }


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--host', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = verify(args.host)
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))
