"""Mixed reply wire compatibility, borrowed storage and vectored socket writes."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class MixedReplyTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        build = tempfile.TemporaryDirectory()
        cls.addClassCleanup(build.cleanup)
        cls.binary = Path(build.name) / 'mixed-reply'
        command = [os.environ.get('CXX', 'g++'), '-std=c++20', '-O2', '-Wall',
                   '-Wextra', '-Werror', '-pthread', '-Wl,--wrap=sendmsg',
                   '-Wl,--wrap=__sendmsg64',
                   '-I', str(ROOT), '-I', str(ROOT / 'server/external_libs'),
                   '-I', os.environ.get('EIGEN_INCLUDE_DIR', '/usr/include/eigen3')]
        command += shlex.split(os.environ.get('CXXFLAGS', ''))
        command += [str(ROOT / 'server' / source) for source in
                    ('tests/mixed_reply.cpp', 'network/websocket.cpp',
                     'network/sha1.cpp', 'network/base64.cpp', 'utilities/rate_tracker.cpp')]
        subprocess.run(command + ['-o', str(cls.binary)], check=True, timeout=120)

    def run_case(self, case):
        result = subprocess.run([str(self.binary), case], capture_output=True,
                                text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_wire_format_and_borrowed_storage(self): self.run_case('serialization')
    def test_owned_and_borrowed_fields(self): self.run_case('ownership')
    def test_driver_reference_returns(self): self.run_case('reference-returns')
    def test_frame_and_size_boundaries(self): self.run_case('boundaries')
    def test_partial_writes_and_interrupts(self): self.run_case('partial')
    def test_zero_writes_and_disconnects(self): self.run_case('failures')
    def test_size_and_descriptor_limits(self): self.run_case('limits')


if __name__ == '__main__':
    unittest.main()
