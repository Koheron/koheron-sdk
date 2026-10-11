"""Host deployment commands against the compiled C++ API and private service fixture."""
import json
from pathlib import Path
import subprocess
import sys
import unittest

from native_fixture import NativeFixture

MANAGEMENT = Path(__file__).resolve().parents[3] / 'python/koheron/management.py'


class NativeSdkTest(NativeFixture):
    def setUp(self):
        super().setUp()
        (self.root / 'manifest').write_text('board=red-pitaya\n')
        self.start_api()

    def command(self, *args):
        result = subprocess.run([sys.executable, str(MANAGEMENT),
            '--host', f'127.0.0.1:{self.port}', '--timeout', '5', '--format', 'json', *args],
            text=True, capture_output=True, timeout=8)
        self.assertEqual(result.stderr, '')
        return result.returncode, json.loads(result.stdout)

    def test_deployment_returns_confirmed_live_identity(self):
        code, result = self.command('deploy', str(self.package))
        self.assertEqual(code, 0, result)
        value = result['deployment']
        self.assertTrue(value['ready'])
        self.assertEqual((value['name'], value['version']), ('new', '2'))
        self.assertEqual(value['status']['health']['instrument_service']['state'], 'active')
        self.assertEqual((self.live / '.instrument-name').read_text().strip(), 'new')

    def test_failed_start_reports_actual_rollback_and_preserves_previous_instrument(self):
        self.save_state(fail_new=True)
        code, result = self.command('deploy', str(self.package))
        self.assertEqual(code, 1)
        self.assertEqual(result['error']['code'], 'start_failed')
        self.assertEqual(result['error']['rollback'], 'restored')
        self.assert_old()

    def test_status_and_empty_logs_do_not_change_service_or_files(self):
        code, result = self.command('status')
        self.assertEqual(code, 0, result)
        self.assertEqual(result['status']['instruments']['live_instrument']['name'], 'old')
        code, result = self.command('logs')
        self.assertEqual(code, 0, result)
        self.assertEqual(result['logs']['entries'], [])
        self.assertFalse(any(line.startswith(('start ', 'stop ', 'restart ')) for line in self.operations()))
        self.assert_old()


if __name__ == '__main__':
    unittest.main()
