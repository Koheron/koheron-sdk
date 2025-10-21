#!/usr/bin/env python3
import socket, struct

HOST = "192.168.1.84"
PORT = 36000

def recv_all(sock, n):
    data = bytearray()
    while len(data) < n:
        chunk = sock.recv(n - len(data))
        if not chunk:
            raise ConnectionError("Socket closed during recv")
        data.extend(chunk)
    return bytes(data)

def recv_string(sock):
    # Header: reserved (u32), driver (u16), op (u16), length (u32), big-endian
    header = recv_all(sock, struct.calcsize(">IHHI"))
    reserved, driver, op, length = struct.unpack(">IHHI", header)
    if reserved != 0:
        raise ValueError(f"reserved != 0 ({reserved})")
    payload = recv_all(sock, length)
    return payload.decode("utf-8", errors="strict")

def send_string(sock, driver, op, msg):
    payload = msg.encode("utf-8")
    header = struct.pack(">IHHI", 0, driver, op, len(payload))
    sock.sendall(header + payload)

with socket.create_connection((HOST, PORT), timeout=5.0) as sock:
    sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 16384)

    for i in range(10):
        msg = f"hello {i}!!!!\n"
        send_string(sock, 0xFFFF, 0, msg)   # matches AppExecutor (driver=0xFFFF, op=0)
        print(recv_string(sock), end="")