"""Opt-in host callback stress; run with sanitizer binaries and diagnostic logs."""
import concurrent.futures
import json
from pathlib import Path
import socket
import time

from native_fixture import NativeFixture

fixture = NativeFixture()
fixture.setUp()
# Service-state reads stay private and cheap; this run targets HTTP lifetimes.
fixture.systemctl = Path('/bin/true')
try:
    fixture.start_api()

    def reader(index):
        end = time.monotonic() + 30
        count = 0
        while time.monotonic() < end:
            path = ['/api/instruments/details', '/api/system/build',
                    '/api/logs/koheron/instrument/bookmark'][count % 3]
            status, body, _ = fixture.request(path)
            assert status == 200, (status, body)
            data = json.loads(body)
            if path.endswith('/details'): assert data['live_instrument']['name'] == 'old'
            count += 1
        return count

    with concurrent.futures.ThreadPoolExecutor(max_workers=32) as pool:
        readers = [pool.submit(reader, i) for i in range(32)]
        for _ in range(500):
            with socket.create_connection(('127.0.0.1', fixture.port), timeout=10) as connection:
                connection.sendall(b'POST /api/instruments/upload HTTP/1.1\r\nHost: localhost\r\nContent-Length: 99999\r\n'
                    b'Content-Type: multipart/form-data; boundary=boundary\r\n\r\n--boundary\r\n'
                    b'Content-Disposition: form-data; name="interrupted.zip"; filename="interrupted.zip"\r\n\r\npartial')
        counts = [future.result() for future in readers]
    time.sleep(.1)
    assert not list(fixture.store.glob('.upload-*'))
    print(json.dumps({'clients': 32, 'duration_s': 30, 'successful_requests': sum(counts),
                      'aborted_uploads': 500, 'leftovers': []}))
finally:
    fixture.doCleanups()
