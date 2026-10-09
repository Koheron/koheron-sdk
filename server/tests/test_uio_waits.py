"""Real poll/eventfd cancellation and UIO ownership on synthetic devices."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest
from uio_fixture import prepare_uio_fixture

ROOT = Path(__file__).resolve().parents[2]
HEADER = ROOT / 'server/drivers/uio.hpp'

class UioWaitsTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.directory.cleanup)
        cls.root = Path(cls.directory.name)
        prepare_uio_fixture(cls.root, HEADER)
        cls.binary = cls.root / 'uio-test'
        command = [os.environ.get('CXX', 'g++'), '-std=c++23', '-O2', '-Wall',
                   '-Wextra', '-Werror', '-fno-exceptions', '-pthread',
                   '-I', str(cls.root), '-I', str(ROOT)]
        command += shlex.split(os.environ.get('CXXFLAGS', ''))
        command += [str(ROOT / 'server/tests/uio_waits.cpp'), '-Wl,--wrap=eventfd',
                    '-Wl,--wrap=write', '-Wl,--wrap=poll',
                    '-Wl,--wrap=__poll_chk',
                    '-o', str(cls.binary)]
        subprocess.run(command, check=True, timeout=90)

    def run_case(self, case):
        result = subprocess.run([str(self.binary), str(self.root), case],
                                capture_output=True, text=True, timeout=8)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_default_listener_waits_for_interrupt(self):
        self.run_case('default')

    def test_cancel_wakes_finite_wait_without_callback(self):
        self.run_case('finite')

    def test_cancel_wakes_infinite_wait_without_callback(self):
        self.run_case('infinite')

    def test_restart_discards_old_wakeups(self):
        self.run_case('restart')

    def test_restart_waits_for_in_flight_cancellation(self):
        self.run_case('concurrent_restart')

    def test_synchronous_waits_remain_independent(self):
        self.run_case('sync')

    def test_move_stops_listener_before_transfer(self):
        self.run_case('move_active')

    def test_mapping_ownership_moves_with_descriptor(self):
        self.run_case('move_map')

    def test_reopen_stops_old_listener(self):
        self.run_case('reopen')

    def test_eventfd_failure_is_retryable(self):
        self.run_case('eventfd_failure')

if __name__ == '__main__':
    unittest.main()
