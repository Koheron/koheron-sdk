"""Real poll/eventfd cancellation and UIO ownership on synthetic devices."""
from pathlib import Path
import tempfile
import unittest
from cpp_fixture import ROOT, compile_cpp, run_cpp
from uio_fixture import prepare_uio_fixture

HEADER = ROOT / 'server/drivers/uio.hpp'

class UioWaitsTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.directory.cleanup)
        cls.root = Path(cls.directory.name)
        prepare_uio_fixture(cls.root, HEADER)
        cls.binary = cls.root / 'uio-test'
        cls.binary = compile_cpp(cls, 'server/tests/uio_waits.cpp',
            flags=('-fno-exceptions', '-pthread', '-Wl,--wrap=eventfd',
                   '-Wl,--wrap=write', '-Wl,--wrap=poll', '-Wl,--wrap=__poll_chk'),
            includes=(cls.root,), output=cls.binary)

    def test_cases(self):
        for case in (
            'finite', 'infinite', 'default', 'eventfd_failure', 'move_map', 'callbacks',
            'move_active', 'reopen', 'restart', 'concurrent_restart', 'sync',
        ):
            with self.subTest(case=case):
                run_cpp(self, self.binary, self.root, case, timeout=8)


if __name__ == '__main__':
    unittest.main()
