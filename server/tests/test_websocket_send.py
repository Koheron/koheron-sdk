"""WebSocket send regressions using production framing and real local sockets."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class WebSocketSendTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        build = tempfile.TemporaryDirectory()
        cls.addClassCleanup(build.cleanup)
        cls.binary = Path(build.name) / 'websocket-send'
        command = [os.environ.get('CXX', 'g++'), '-std=c++23', '-O2', '-Wall',
                   '-Wextra', '-Werror', '-pthread', '-Wl,--wrap=sendmsg',
                   '-I', str(ROOT), '-I', str(ROOT / 'server/external_libs'),
                   '-I', os.environ.get('EIGEN_INCLUDE_DIR', '/usr/include/eigen3')]
        command += shlex.split(os.environ.get('CXXFLAGS', ''))
        command += [str(ROOT / 'server' / source) for source in
                    ('tests/websocket_send.cpp', 'network/websocket.cpp',
                     'network/sha1.cpp', 'network/base64.cpp')]
        subprocess.run(command + ['-o', str(cls.binary)], check=True, timeout=120)

    def run_case(self, case):
        result = subprocess.run([str(self.binary), case], capture_output=True,
                                text=True, timeout=15)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_length_boundaries_and_borrowed_storage(self): self.run_case('boundaries')
    def test_fragmented_replies(self): self.run_case('fragmented')
    def test_partial_writes_and_interrupts(self): self.run_case('partial')
    def test_send_failures_and_disconnect(self): self.run_case('failures')
    def test_single_range_limit_and_close_frame(self): self.run_case('single-limit')


if __name__ == '__main__':
    unittest.main()
