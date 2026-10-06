"""WebSocket unmasking regressions; the same C++ harness runs on ARM hardware."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class WebSocketMaskTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        build = tempfile.TemporaryDirectory()
        cls.addClassCleanup(build.cleanup)
        cls.binary = Path(build.name) / 'websocket-mask'
        command = [os.environ.get('CXX', 'g++'), '-std=c++20', '-O2', '-Wall',
                   '-Wextra', '-Werror', '-pthread', '-I', str(ROOT),
                   '-I', str(ROOT / 'server/external_libs'),
                   '-I', os.environ.get('EIGEN_INCLUDE_DIR', '/usr/include/eigen3')]
        command += shlex.split(os.environ.get('CXXFLAGS', ''))
        command += [str(ROOT / 'server' / source) for source in
                    ('tests/websocket_mask.cpp', 'network/websocket.cpp',
                     'network/sha1.cpp', 'network/base64.cpp')]
        subprocess.run(command + ['-o', str(cls.binary)], check=True, timeout=120)

    def run_case(self, case):
        result = subprocess.run([str(self.binary), case], capture_output=True,
                                text=True, timeout=60)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_lengths_alignments_phases_and_in_place(self): self.run_case('boundaries')
    def test_no_reads_or_writes_past_buffer_end(self): self.run_case('guard-pages')
    def test_wire_frames_and_header_payload_split(self): self.run_case('receive')


if __name__ == '__main__':
    unittest.main()
