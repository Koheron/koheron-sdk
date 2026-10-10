import os
from pathlib import Path
import subprocess

from native_fixture import BIN_DIR, NativeFixture


class NativeFaultTest(NativeFixture):
    @classmethod
    def setUpClass(cls):
        super().setUpClass()
        cls.library = BIN_DIR / 'fail-io.so'
        subprocess.run([os.environ.get('CXX', 'g++-15'), '-std=c++23', '-shared', '-fPIC',
            str(Path(__file__).with_name('fail_io.cpp')), '-ldl', '-o', str(cls.library)], check=True)

    def inject(self, fault):
        self.environment.update(LD_PRELOAD=str(self.library), NATIVE_IO_FAULT=fault)
        result = self.install()
        self.assertNotEqual(result.returncode, 0)
        return result

    def test_extract_disk_full_does_not_stop_instrument(self):
        self.inject('extract'); self.assert_old()
        self.assertFalse(any(line.startswith('stop ') for line in self.operations()))
        self.assertEqual(list(self.root.glob('.instrument-*')), [])

    def test_swap_failure_restores_and_restarts_previous(self):
        self.inject('swap'); self.assert_old()
        self.assertTrue(self.load_state()['active'])
        self.assertEqual(list(self.root.glob('.instrument-*')), [])

    def test_restore_failure_retains_backup_and_reports_path(self):
        self.save_state(fail_new=True)
        result = self.inject('restore')
        backup = list(self.root.glob('.instrument-*/previous'))
        self.assertEqual(len(backup), 1)
        self.assertEqual((backup[0] / 'serverd').read_bytes(), b'old executable')
        self.assertIn(str(backup[0]), result.stderr)
        self.assertFalse(self.load_state()['active'])

    def boot(self, fault):
        self.environment.update(LD_PRELOAD=str(self.library), NATIVE_IO_FAULT=fault)
        (self.store / 'default').write_text('new.zip\n')
        result = subprocess.run([str(BIN_DIR / 'koheron-install'), '--extract-default',
            '--instruments', str(self.store), '--live', str(self.live), '--loader', 'overlay'],
            env=self.environment, capture_output=True, text=True, timeout=5)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.operations(), [])
        return result

    def test_boot_disk_full_preserves_previous_extraction(self):
        self.boot('extract'); self.assert_old()
        self.assertEqual(list(self.root.glob('.instrument-*')), [])

    def test_boot_swap_failure_restores_previous_extraction(self):
        self.boot('swap'); self.assert_old()
        self.assertEqual(list(self.root.glob('.instrument-*')), [])

    def test_boot_restore_failure_retains_backup_and_reports_path(self):
        result = self.boot('swap-restore')
        backup = list(self.root.glob('.instrument-*/previous'))
        self.assertEqual(len(backup), 1)
        self.assertEqual((backup[0] / 'serverd').read_bytes(), b'old executable')
        self.assertIn(str(backup[0]), result.stderr)
