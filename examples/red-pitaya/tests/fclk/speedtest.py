#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import socket, struct, threading, time, random, string
from dataclasses import dataclass, field

# ====== CONFIG ======
HOST = "192.168.1.84"
PORT = 36000
SESSIONS = 10
MESSAGES = 10
BCAST_RATE = 0.2
POLLS_PER_ITER = 1
MIN_LEN = 0
MAX_LEN = 1024
UNICODE_RATE = 0.5
SOCK_TIMEOUT = 5.0
VERBOSE = False
# ====================

# ---------- helpers (UNCHANGED) ----------
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
# -----------------------------------------

HEADER_BYTES = 12  # 4 + 2 + 2 + 4

def new_session():
    s = socket.create_connection((HOST, PORT), timeout=SOCK_TIMEOUT)
    s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 16384)
    return s

def rand_ascii(n):
    alphabet = string.ascii_letters + string.digits + " _-.:,;!@#$%^&*()[]{}<>?/\\|+=~"
    return "".join(random.choice(alphabet) for _ in range(n))

@dataclass
class Stats:
    sent_dm: int = 0
    sent_bcast: int = 0
    recv_msgs: int = 0
    errors: int = 0
    # byte counters (aggregate across all threads)
    bytes_up: int = 0           # header + payload sent to server
    bytes_down: int = 0         # header + payload received from server
    payload_up: int = 0         # payload-only sent
    payload_down: int = 0       # payload-only received
    lock: threading.Lock = field(default_factory=threading.Lock)
    def add(self, **kw):
        with self.lock:
            for k, v in kw.items():
                setattr(self, k, getattr(self, k) + v)

# wrappers that count bytes but still use your helpers
def send_counting(sock, driver, op, msg, stats: Stats):
    p = msg.encode("utf-8")
    send_string(sock, driver, op, msg)
    stats.add(bytes_up=HEADER_BYTES + len(p), payload_up=len(p))

def recv_counting(sock, stats: Stats):
    s = recv_string(sock)
    p = s.encode("utf-8")
    stats.add(bytes_down=HEADER_BYTES + len(p), payload_down=len(p))
    return s

def poll_once(sock, stats: Stats):
    # server drains optional payload for op=11; we send 1-byte pad to be extra-safe
    send_counting(sock, 0xFFFF, 11, "\n", stats)
    return recv_counting(sock, stats)

def worker(idx, usernames, barrier, stats: Stats):
    rng = random.Random(idx * 1337 + int(time.time()))
    name = usernames[idx]
    s = None
    try:
        s = new_session()

        # Register (op=20)
        send_counting(s, 0xFFFF, 20, name, stats)
        resp = recv_counting(s, stats)
        if not resp.startswith("ok: user="):
            stats.add(errors=1)
            if VERBOSE: print(f"[{name}] register failed: {resp}")
            return
        if VERBOSE: print(f"[{name}] registered")

        barrier.wait()

        for i in range(MESSAGES):
            # Build message
            n = rng.randint(MIN_LEN, MAX_LEN)
            msg = rand_ascii(n)
            if rng.random() < UNICODE_RATE:
                msg += " 🚀汉字üä—"

            if rng.random() < BCAST_RATE:
                # Broadcast (op=22)
                send_counting(s, 0xFFFF, 22, msg, stats)
                ack = recv_counting(s, stats)
                if ack.startswith("ok:"):
                    stats.add(sent_bcast=1)
                else:
                    stats.add(errors=1)
                    if VERBOSE: print(f"[{name}] bcast ack error: {ack}")
            else:
                # DM to random other user (op=21)
                j = idx
                while j == idx:
                    j = rng.randrange(len(usernames))
                target = usernames[j]
                send_counting(s, 0xFFFF, 21, f"{target}\n{msg},", stats)
                ack = recv_counting(s, stats)
                if ack.startswith("ok:"):
                    stats.add(sent_dm=1)
                else:
                    stats.add(errors=1)
                    if VERBOSE: print(f"[{name}] dm ack error: {ack}")

            # Poll a few times to drain inbox
            for _ in range(POLLS_PER_ITER):
                got = poll_once(s, stats)
                if got != "none":
                    stats.add(recv_msgs=1)
                    if VERBOSE and VERBOSE >= 2:
                        print(f"[{name}] RECV: {got[:120]}")
                else:
                    time.sleep(0.005)

            time.sleep(rng.uniform(0.0, 0.01))

        # Final drain
        empties = 0
        while empties < 40:
            got = poll_once(s, stats)
            if got == "none":
                empties += 1
                time.sleep(0.005)
            else:
                stats.add(recv_msgs=1)
                empties = 0

    except Exception as e:
        stats.add(errors=1)
        if VERBOSE: print(f"[{name}] exception: {e}")
    finally:
        try:
            if s is not None:
                s.close()
        except:
            pass

def main():
    print(f"Starting {SESSIONS} sessions to {HOST}:{PORT} …")
    usernames = [f"user{i:05d}" for i in range(SESSIONS)]
    barrier = threading.Barrier(SESSIONS)
    stats = Stats()

    threads = []
    t0 = time.perf_counter()
    for i in range(SESSIONS):
        t = threading.Thread(target=worker, args=(i, usernames, barrier, stats), daemon=True)
        t.start()
        threads.append(t)
    for t in threads:
        t.join()
    dt = max(1e-9, time.perf_counter() - t0)

    # Throughput calculations
    with stats.lock:
        sent_total = stats.sent_dm + stats.sent_bcast
        mib = 1024.0 * 1024.0
        mb  = 1000.0 * 1000.0

        ingress_mib_s = (stats.bytes_up / mib) / dt
        egress_mib_s  = (stats.bytes_down / mib) / dt
        ingress_mb_s  = (stats.bytes_up / mb) / dt
        egress_mb_s   = (stats.bytes_down / mb) / dt

        ingress_payload_mib_s = (stats.payload_up / mib) / dt
        egress_payload_mib_s  = (stats.payload_down / mib) / dt

        print("\n=== SUMMARY ===")
        print(f"sessions            : {SESSIONS}")
        print(f"msgs/session        : {MESSAGES}")
        print(f"sent (dm)           : {stats.sent_dm}")
        print(f"sent (broadcast)    : {stats.sent_bcast}")
        print(f"sent (total)        : {sent_total}")
        print(f"received msgs       : {stats.recv_msgs}")
        print(f"errors              : {stats.errors}")
        print(f"elapsed (s)         : {dt:.3f}")

        print("\n--- Server Ingress (client → server) ---")
        print(f"on-wire: {ingress_mib_s:.2f} MiB/s  ({ingress_mb_s:.2f} MB/s)")
        print(f"payload: {ingress_payload_mib_s:.2f} MiB/s")

        print("\n--- Server Egress (server → client) ---")
        print(f"on-wire: {egress_mib_s:.2f} MiB/s  ({egress_mb_s:.2f} MB/s)")
        print(f"payload: {egress_payload_mib_s:.2f} MiB/s")

if __name__ == "__main__":
    main()
