#!/usr/bin/env python3
"""Check the FFT custom executor's echo operation without restarting it."""
import os
import socket
import struct


def recv_exact(sock, size):
    data = bytearray()
    while len(data) < size:
        chunk = sock.recv(size - len(data))
        if not chunk:
            raise ConnectionError('Server disconnected before completing its reply')
        data.extend(chunk)
    return bytes(data)


def main():
    payload = b'hello!!!!\n'
    request = struct.pack('>IHHI', 0, 0xFFFF, 0, len(payload)) + payload
    with socket.create_connection((os.getenv('HOST', '192.168.1.100'), 36000), timeout=5) as sock:
        for _ in range(10):
            sock.sendall(request)
            reserved, driver, operation, size = struct.unpack('>IHHI', recv_exact(sock, 12))
            assert (reserved, driver, operation) == (0, 0xFFFF, 0), 'Unexpected reply header'
            assert recv_exact(sock, size) == payload, 'Echo reply changed the payload'


if __name__ == '__main__':
    main()
