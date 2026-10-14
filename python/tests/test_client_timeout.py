"""Connection readiness must include a bound on a stalled server handshake."""
import socket
import threading
import time
import unittest
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from koheron import KoheronClient


class ClientTimeoutTest(unittest.TestCase):
    def test_stalled_version_reply_obeys_timeout(self):
        with socket.socket() as listener:
            listener.bind(('127.0.0.1', 0))
            listener.listen(1)
            listener.settimeout(1)
            stopped = threading.Event()

            def server():
                try:
                    connection, _ = listener.accept()
                except OSError:
                    return
                with connection:
                    connection.recv(8)
                    stopped.wait(1)

            worker = threading.Thread(target=server)
            worker.start()
            started = time.monotonic()
            try:
                with self.assertRaises(socket.timeout):
                    KoheronClient('127.0.0.1', listener.getsockname()[1], timeout=0.05)
                self.assertLess(time.monotonic() - started, 0.5)
            finally:
                stopped.set()
                worker.join(timeout=1)
                self.assertFalse(worker.is_alive())


if __name__ == '__main__':
    unittest.main()
