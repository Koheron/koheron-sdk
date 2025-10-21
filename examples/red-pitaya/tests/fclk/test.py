#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import socket, struct, time

HOST = "192.168.1.84"
PORT = 36000

# ---- helpers (unchanged) ----
def recv_all(sock, n):
    data = bytearray()
    while len(data) < n:
        chunk = sock.recv(n - len(data))
        if not chunk:
            raise ConnectionError("Socket closed during recv")
        data.extend(chunk)
    return bytes(data)

def recv_string(sock):
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
# -----------------------------

def poll_once(sock, label):
    send_string(sock, 0xFFFF, 11, "\n")  # server drains optional payload
    msg = recv_string(sock)
    if msg != "none":
        print(f"{label} NOTIFY:", msg)
    return msg

def main():
    # Two sessions
    sA = socket.create_connection((HOST, PORT), timeout=5.0)
    sB = socket.create_connection((HOST, PORT), timeout=5.0)
    for s in (sA, sB):
        s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 16384)

    # Register users (creates queues)
    send_string(sA, 0xFFFF, 20, "alice")
    print("A REG:", recv_string(sA))
    send_string(sB, 0xFFFF, 20, "bob")
    print("B REG:", recv_string(sB))

    # Alice -> Bob (DM)
    send_string(sA, 0xFFFF, 21, "bob\nhey Bob, from Alice!")
    print("A DM:", recv_string(sA))
    for _ in range(40):
        if poll_once(sB, "B") != "none":
            break
        time.sleep(0.05)

    # Bob -> broadcast
    send_string(sB, 0xFFFF, 22, "hello everyone! - Bob")
    print("B BCAST:", recv_string(sB))
    for _ in range(40):
        gotA = poll_once(sA, "A")
        gotB = poll_once(sB, "B")
        if gotA != "none" and gotB != "none":
            break
        time.sleep(0.05)

    sA.close()
    sB.close()

if __name__ == "__main__":
    main()
