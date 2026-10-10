"""Readiness must imply a usable HTTP server; inherited sockets survive restarts."""
import os
import socket
import subprocess
import sys

from native_fixture import BIN_DIR, NativeFixture


class NativeActivationTest(NativeFixture):
    def test_version_preflight_needs_no_board_files_or_services(self):
        result = subprocess.run([str(BIN_DIR / 'koheron-api'), '--version'], capture_output=True, timeout=2)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn(b'C++23', result.stdout)

    def test_notify_and_reuse_inherited_socket(self):
        api_path, notify_path = self.root / 'api.sock', self.root / 'notify.sock'
        with socket.socket(socket.AF_UNIX) as listener, socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM) as notify:
            listener.bind(str(api_path)); listener.listen()
            notify.bind(str(notify_path)); notify.settimeout(3)
            wrapper = ('import os,sys; fd=int(sys.argv[1]); os.dup2(fd,3,inheritable=True); '
                       'os.environ.update(LISTEN_FDS="1",LISTEN_PID=str(os.getpid())); '
                       'os.execv(sys.argv[2],sys.argv[2:])')
            for _ in range(2):
                process = subprocess.Popen([sys.executable, '-c', wrapper, str(listener.fileno()),
                    str(BIN_DIR / 'koheron-api'), '--instruments', str(self.store), '--live', str(self.live),
                    '--systemctl', str(self.systemctl)], pass_fds=(listener.fileno(),),
                    env={**self.environment, 'NOTIFY_SOCKET': str(notify_path)},
                    stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
                try:
                    self.assertIn(b'READY=1', notify.recv(4096))
                    with socket.socket(socket.AF_UNIX) as client:
                        client.settimeout(2); client.connect(str(api_path))
                        client.sendall(b'GET /api/instruments/details HTTP/1.0\r\n\r\n')
                        response = b''
                        while chunk := client.recv(4096): response += chunk
                        self.assertIn(b'200 OK', response)
                        self.assertIn(b'loaded-version', response)
                finally:
                    process.terminate()
                    _, errors = process.communicate(timeout=3)
                self.assertEqual(process.returncode, 0, errors)
                self.assertIn(b'STOPPING=1', notify.recv(4096))
                self.assertTrue(api_path.exists())
