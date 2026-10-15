"""Full FPGA loading flows with fake kernel operations and real temporary files."""
import tempfile
import unittest
from cpp_fixture import compile_cpp, run_cpp


class FpgaManagerTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.binary = compile_cpp(cls, 'server/tests/fpga_manager.cpp', 'server/hardware/fpga_manager.cpp',
            flags=('-Wpedantic', '-fno-exceptions', '-DINSTRUMENT_NAME="test"',
                   '-Wl,--wrap=open', '-Wl,--wrap=open64', '-Wl,--wrap=write',
                   '-Wl,--wrap=close', '-Wl,--wrap=rmdir', '-Wl,--wrap=mount', '-Wl,--wrap=mknod'))

    def run_case(self, case):
        with tempfile.TemporaryDirectory() as directory:
            run_cpp(self, self.binary, directory, case)

    def test_cases(self):
        for case in (
            'backend-retry', 'backend-error', 'overlay-bridges', 'overlay-busy',
            'overlay-cleanup-error', 'overlay-good', 'overlay-configfs-error',
            'overlay-mounted', 'overlay-mount-error', 'overlay-empty-payload',
            'overlay-false-applied', 'overlay-copy-error', 'overlay-flags-error',
            'overlay-bad-uio', 'no-backend', 'overlay-missing-interface',
            'overlay-missing-payload', 'overlay-apply-error', 'overlay-inspection-error',
            'overlay-replaces-old', 'overlay-program-close', 'overlay-program-short',
            'overlay-program-error', 'overlay-status-error', 'overlay-state-missing',
            'xdev-write-close', 'xdev-empty-bit', 'xdev-state-error', 'xdev-missing-bit',
            'xdev-good', 'xdev-write-short', 'xdev-state-missing', 'xdev-write-error',
        ):
            with self.subTest(case=case):
                self.run_case(case)
    def test_failed_programming_state(self):
        for state in ('error', 'prefix', 'empty'):
            with self.subTest(state=state):
                self.run_case('overlay-state-' + state)


if __name__ == '__main__':
    unittest.main()
