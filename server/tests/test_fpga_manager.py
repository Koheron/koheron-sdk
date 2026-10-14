"""Full FPGA loading flows with fake kernel operations and real temporary files."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class FpgaManagerTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.build = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.build.cleanup)
        cls.binary = Path(cls.build.name) / 'fpga-manager'
        command = [os.environ.get('CXX', 'g++'), '-std=c++23', '-O2', '-Wall', '-Wextra',
                   '-Werror', '-Wpedantic', '-fno-exceptions', '-DINSTRUMENT_NAME="test"', '-I', str(ROOT)]
        command += shlex.split(os.environ.get('CXXFLAGS', ''))
        command += [str(ROOT / 'server/tests/fpga_manager.cpp'),
                    str(ROOT / 'server/hardware/fpga_manager.cpp'),
                    '-Wl,--wrap=open', '-Wl,--wrap=open64', '-Wl,--wrap=write',
                    '-Wl,--wrap=close', '-Wl,--wrap=rmdir', '-Wl,--wrap=mount', '-Wl,--wrap=mknod',
                    '-o', str(cls.binary)]
        subprocess.run(command, check=True, timeout=90)

    def run_case(self, case):
        with tempfile.TemporaryDirectory() as directory:
            result = subprocess.run([str(self.binary), directory, case],
                                    capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_missing_backend_is_recoverable(self): self.run_case('no-backend')
    def test_backend_discovery_filesystem_error(self): self.run_case('backend-error')
    def test_backend_can_appear_before_retry(self): self.run_case('backend-retry')
    def test_complete_overlay_flow(self): self.run_case('overlay-good')
    def test_configfs_mount(self): self.run_case('overlay-mounted')
    def test_previous_overlay_removed_and_region_restored(self): self.run_case('overlay-replaces-old')
    def test_malformed_uio_numbers_do_not_throw(self): self.run_case('overlay-bad-uio')
    def test_missing_live_payload(self): self.run_case('overlay-missing-payload')
    def test_empty_live_payload(self): self.run_case('overlay-empty-payload')
    def test_firmware_copy_failure(self): self.run_case('overlay-copy-error')
    def test_bridge_designs_are_rejected(self): self.run_case('overlay-bridges')
    def test_missing_firmware_interface(self): self.run_case('overlay-missing-interface')
    def test_configfs_filesystem_error(self): self.run_case('overlay-configfs-error')
    def test_configfs_mount_error(self): self.run_case('overlay-mount-error')
    def test_overlay_inspection_error(self): self.run_case('overlay-inspection-error')
    def test_busy_overlay_retries_and_restores_region(self): self.run_case('overlay-busy')
    def test_flags_write_failure(self): self.run_case('overlay-flags-error')
    def test_program_write_failure(self): self.run_case('overlay-program-error')
    def test_program_short_write_not_replayed(self): self.run_case('overlay-program-short')
    def test_program_close_failure(self): self.run_case('overlay-program-close')
    def test_failed_programming_state(self):
        for state in ('error', 'prefix', 'empty'):
            with self.subTest(state=state):
                self.run_case('overlay-state-' + state)
    def test_unreadable_programming_state(self): self.run_case('overlay-state-missing')
    def test_overlay_apply_write_failure(self): self.run_case('overlay-apply-error')
    def test_false_applied_overlay_rejected(self): self.run_case('overlay-false-applied')
    def test_unapplied_overlay_rejected(self): self.run_case('overlay-status-error')
    def test_cleanup_failure_preserves_programming_error(self): self.run_case('overlay-cleanup-error')
    def test_xdevcfg_precedence_and_binary_payload(self): self.run_case('xdev-good')
    def test_xdevcfg_missing_bitstream(self): self.run_case('xdev-missing-bit')
    def test_xdevcfg_empty_bitstream(self): self.run_case('xdev-empty-bit')
    def test_xdevcfg_write_failure(self): self.run_case('xdev-write-error')
    def test_xdevcfg_short_write_not_replayed(self): self.run_case('xdev-write-short')
    def test_xdevcfg_close_failure(self): self.run_case('xdev-write-close')
    def test_xdevcfg_incomplete_programming(self): self.run_case('xdev-state-error')
    def test_xdevcfg_unreadable_status(self): self.run_case('xdev-state-missing')


if __name__ == '__main__':
    unittest.main()
