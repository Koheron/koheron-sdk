"""Host deployment commands against the compiled C++ API and private service fixture."""
import json
from contextlib import closing, redirect_stderr
import importlib.util
import io
from pathlib import Path
import subprocess
import sys
import unittest
from unittest.mock import patch

from native_fixture import NativeFixture

MANAGEMENT = Path(__file__).resolve().parents[3] / 'python/koheron/management.py'
LOG_STREAM = MANAGEMENT.with_name('log_stream.py')


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

    def test_host_log_reader_receives_real_websocket_heartbeats_without_http_polling(self):
        spec = importlib.util.spec_from_file_location('sdk_log_stream', LOG_STREAM)
        log_stream = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(log_stream)
        with patch.object(log_stream.requests, 'Session') as session, redirect_stderr(io.StringIO()):
            session.return_value.__enter__.return_value.get.side_effect = AssertionError('Unexpected HTTP fallback')
            with closing(log_stream.log_batches(
                    f'http://127.0.0.1:{self.port}/api/logs/koheron/instrument', 'missing')) as reader:
                first, second = next(reader), next(reader)
                self.assertEqual(first, dict(type='logs', cursor=None, entries=[], reset=True))
                self.assertEqual(second, dict(type='logs', cursor=None, entries=[], reset=False))
            session.return_value.__enter__.return_value.get.assert_not_called()
        self.assertFalse(any(line.startswith(('start ', 'stop ', 'restart ')) for line in self.operations()))
        self.assert_old()


if __name__ == '__main__':
    unittest.main()
