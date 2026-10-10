import os
from pathlib import Path
import subprocess
import json

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

    def test_capacity_preflight_rejects_before_stopping(self):
        self.environment.update(LD_PRELOAD=str(self.library), NATIVE_IO_FAULT='capacity')
        self.start_api()
        status, body, _ = self.request('/api/instruments/preflight/new')
        self.assertEqual(status, 200)
        self.assertEqual(json.loads(body)['code'], 'insufficient_space')
        status, body, _ = self.request('/api/instruments/activate/new', b'', method='POST')
        self.assertEqual(status, 507)
        self.assertEqual(json.loads(body)['code'], 'insufficient_space')
        self.assert_old()
        self.assertFalse(any(line.startswith('stop ') for line in self.operations()))

    def test_directory_sync_failure_reports_actual_default(self):
        self.environment.update(LD_PRELOAD=str(self.library), NATIVE_IO_FAULT='sync-directory')
        self.start_api()
        self.assertEqual(self.request('/api/instruments/default/new', b'', method='POST')[0], 500)
        actual = (self.store / 'default').read_text().strip().removesuffix('.zip')
        inventory = json.loads(self.request('/api/instruments/details')[1])['instruments']
        self.assertEqual([i['name'] for i in inventory if i['is_default']], [actual])
        self.assertEqual(list(self.store.glob('.preference-*')), [])

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
