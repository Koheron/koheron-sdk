"""Exercise the host commands over HTTP, without a board or FPGA."""
import contextlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
import time
import unittest
from unittest.mock import patch
import zipfile

from click.testing import CliRunner
from koheron.cli import cli
from koheron import management

SDK = Path(__file__).resolve().parents[2]


class ManagementCliTest(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.archive = Path(temporary.name) / 'fft.zip'
        with zipfile.ZipFile(self.archive, 'w') as archive:
            archive.writestr('version', '0.3.0\n')
            archive.writestr('instrument.json', json.dumps({'board': 'red-pitaya'}))
        self.events = []
        self.board = 'red-pitaya'
        self.preflight = {'ready': True, 'warnings': []}
        self.activation_error = None
        self.ready = True
        self.busy = False
        self.stale_health = 0
        self.delay = 0
        self.upload_error = False
        self.response_override = None
        self.drip = 0
        self.body_delay = 0
        self.payload = None
        fixture = self

        class Handler(BaseHTTPRequestHandler):
            def log_message(self, *args):
                pass

            def do_GET(self):
                self.route()

            def do_POST(self):
                self.route()

            def route(self):
                length = int(self.headers.get('Content-Length', 0))
                data = self.rfile.read(length)
                fixture.events.append((self.command, self.path))
                time.sleep(fixture.delay)
                status = 200
                if self.path == '/api/system/status':
                    value = fixture.status()
                elif self.path == '/api/system/build':
                    value = {'manifest': {'board': fixture.board}, 'release': {}}
                elif self.path == '/api/instruments/upload':
                    fixture.payload = data
                    value = {'message': 'uploaded'}
                    if fixture.upload_error:
                        status, value = 507, {'code': 'insufficient_space', 'message': 'No staging space'}
                elif self.path == '/api/instruments/preflight/fft':
                    value = fixture.preflight
                elif self.path == '/api/instruments/activate/fft':
                    if fixture.activation_error:
                        status, value = 500, fixture.activation_error
                    else:
                        value = {'instrument': 'fft', 'phase': 'succeeded', 'busy': False}
                elif self.path.startswith('/api/logs/koheron?'):
                    value = {'entries': [{'ts': 123, 'msg': 'ready'}], 'cursor': 's=last', 'reset': False}
                else:
                    status, value = 404, {'error': 'not found'}
                if fixture.response_override is not None:
                    value = fixture.response_override
                content = json.dumps(value).encode()
                try:
                    self.send_response(status)
                    self.send_header('Content-Type', 'application/json')
                    self.send_header('Content-Length', str(len(content)))
                    self.end_headers()
                    time.sleep(fixture.body_delay)
                    if fixture.drip:
                        for byte in content:
                            self.wfile.write(bytes([byte]))
                            self.wfile.flush()
                            time.sleep(fixture.drip)
                    else:
                        self.wfile.write(content)
                except (BrokenPipeError, ConnectionResetError):
                    pass

        self.server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
        self.addCleanup(self.server.server_close)
        thread = threading.Thread(target=self.server.serve_forever, kwargs={'poll_interval': 0.01}, daemon=True)
        thread.start()
        self.addCleanup(self.server.shutdown)
        self.host = '127.0.0.1:' + str(self.server.server_port)

    def status(self):
        live = {'name': 'fft', 'version': '0.3.0'} if self.ready else None
        service = 'active' if self.ready else 'activating'
        if self.stale_health:
            service = 'failed'
            self.stale_health -= 1
        return {'type': 'status', 'instruments': {'live_instrument': live},
                'operation': {'busy': self.busy},
                'health': {'instrument_service': {'state': service}}}

    def execute(self, action='deploy', **kwargs):
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            code = management.execute(action, self.host, format='json',
                                      archive=self.archive, **kwargs)
        return code, json.loads(output.getvalue())

    def test_deploy_confirms_version_and_readiness_and_returns(self):
        code, value = self.execute()
        self.assertEqual(code, 0)
        self.assertTrue(value['deployment']['ready'])
        self.assertEqual(value['deployment']['version'], '0.3.0')
        self.assertIn(self.archive.read_bytes(), self.payload)
        self.assertIn(b'name="fft.zip"', self.payload)
        self.assertIn(b'filename="fft.zip"', self.payload)
        self.assertEqual(self.events, [('GET', '/api/system/status'), ('GET', '/api/system/build'),
            ('POST', '/api/instruments/upload'), ('GET', '/api/instruments/preflight/fft'),
            ('POST', '/api/instruments/activate/fft'), ('GET', '/api/system/status')])

    def test_wrong_board_and_busy_operation_fail_before_upload(self):
        for board, busy, expected in (('alpha250', False, 'board_mismatch'), ('red-pitaya', True, 'busy')):
            with self.subTest(expected=expected):
                self.board, self.busy = board, busy
                self.events.clear()
                code, result = self.execute()
                self.assertEqual(code, 1)
                self.assertEqual(result['error']['code'], expected)
                self.assertTrue(all(method == 'GET' for method, _ in self.events))

    def test_fresh_live_identity_waits_for_cached_health_to_refresh(self):
        self.stale_health = 2
        code, result = self.execute(timeout=2)
        self.assertEqual(code, 0, result)
        self.assertTrue(result['deployment']['ready'])
        self.assertEqual(result['deployment']['status']['health']['instrument_service']['state'], 'active')

    def test_preflight_failure_does_not_activate(self):
        self.preflight = {'ready': False, 'code': 'architecture_mismatch', 'message': 'Wrong CPU architecture'}
        code, result = self.execute()
        self.assertEqual(code, 1)
        self.assertEqual(result['error']['code'], 'architecture_mismatch')
        self.assertFalse(any('activate' in path for _, path in self.events))

    def test_upload_failure_preserves_server_error_and_does_not_activate(self):
        self.upload_error = True
        code, result = self.execute()
        self.assertEqual(code, 1)
        self.assertEqual(result['error']['message'], 'No staging space')
        self.assertFalse(any('preflight' in path or 'activate' in path for _, path in self.events))

    def test_activation_failure_reports_rollback(self):
        self.activation_error = {'code': 'start_failed', 'message': 'Instrument start failed', 'rollback': 'succeeded'}
        code, result = self.execute()
        self.assertEqual(code, 1)
        self.assertEqual(result['error'], self.activation_error)

    def test_unready_service_and_slow_network_have_bounded_deadlines(self):
        self.ready = False
        start = time.monotonic()
        code, result = self.execute(timeout=0.1)
        self.assertEqual(code, 1)
        self.assertEqual(result['error']['code'], 'timeout')
        self.assertLess(time.monotonic() - start, 0.8)
        self.events.clear()
        self.delay = 0.5
        start = time.monotonic()
        code, result = self.execute('status', timeout=0.05)
        self.assertEqual(code, 1)
        self.assertEqual(result['error']['code'], 'timeout')
        self.assertLess(time.monotonic() - start, 0.4)

    def test_status_and_logs_are_read_only_and_logs_return_without_following(self):
        for action in ('status', 'logs'):
            code, result = self.execute(action)
            self.assertEqual(code, 0)
            self.assertIn(action, result)
        self.assertTrue(all(method == 'GET' for method, _ in self.events))
        self.assertEqual(self.events[-1][1], '/api/logs/koheron?lines=100')

    def test_trickling_response_body_cannot_extend_deadline(self):
        self.drip = 0.01
        started = time.monotonic()
        code, result = self.execute('status', timeout=0.08)
        self.assertEqual(code, 1)
        self.assertEqual(result['error']['code'], 'timeout')
        self.assertLess(time.monotonic() - started, 0.4)

    def test_headers_and_body_share_one_timeout_budget(self):
        self.delay = 0.2
        self.body_delay = 1
        started = time.monotonic()
        code, result = self.execute('status', timeout=0.3)
        self.assertEqual(code, 1)
        self.assertEqual(result['error']['code'], 'timeout')
        self.assertLess(time.monotonic() - started, 0.45)

    def test_malformed_status_and_logs_return_structured_errors(self):
        status = self.status()
        status['instruments']['live_instrument'] = 'invalid'
        for action, response in (('status', []), ('status', status), ('logs', {'entries': ['invalid']})):
            with self.subTest(action=action, response=response):
                self.response_override = response
                code, result = self.execute(action)
                self.assertEqual(code, 1)
                self.assertEqual(result['error']['code'], 'invalid_response')

    def test_make_status_and_logs_need_no_cfg_build_or_python_install(self):
        for action in ('status', 'logs'):
            result = subprocess.run(['make', '--no-print-directory', action, f'HOST={self.host}',
                                     'FORMAT=json', 'VENV=/nonexistent', 'VIVADO_PATH=/nonexistent'],
                                    cwd=SDK, text=True, capture_output=True, timeout=5)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertTrue(json.loads(result.stdout)['ok'])

    def test_make_deploy_keeps_build_output_off_json_stdout(self):
        version = self.archive.parent / 'version'
        manifest = self.archive.parent / 'instrument.json'
        version.write_text('0.3.0\n')
        manifest.write_text(json.dumps({'board': 'red-pitaya'}))
        result = subprocess.run(['make', '--no-print-directory', 'deploy',
            'CFG=examples/red-pitaya/fft/stemlab-125-14.mk', f'HOST={self.host}', 'FORMAT=json',
            f'TMP={self.archive.parent}/cache', f'INSTRUMENT_ZIP={self.archive}',
            f'INSTRUMENT_FILES={version} {manifest}'],
            cwd=SDK, text=True, capture_output=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertTrue(json.loads(result.stdout)['deployment']['ready'])
        self.assertIn('adding: version', result.stderr)

    def test_invalid_options_and_mismatched_name_do_not_contact_board(self):
        for kwargs in ({'timeout': 0}, {'timeout': float('nan')}, {'name': 'other'}):
            with self.subTest(kwargs=kwargs):
                code, result = self.execute(**kwargs)
                self.assertEqual(code, 1)
                self.assertEqual(result['error']['code'], 'invalid_input')
        self.assertEqual(self.events, [])

    def test_invalid_origins_fail_before_connecting(self):
        for host in ('', 'ftp://board', 'http://user:secret@board', 'http://board/path',
                     'http://board:bad', 'http://board:0', 'http://board:65536'):
            with self.subTest(host=host), patch.object(management.HTTPConnection, 'connect') as connect:
                output = io.StringIO()
                with contextlib.redirect_stdout(output):
                    code = management.execute('status', host, format='json')
                self.assertEqual(code, 1)
                self.assertEqual(json.loads(output.getvalue())['error']['code'], 'invalid_input')
                connect.assert_not_called()

    def test_invalid_package_identity_and_metadata_do_not_contact_board(self):
        for filename, version, metadata in (
            ('fft #1.zip', '0.3.0', {}), ('_fft.zip', '0.3.0', {}), ('fft_.zip', '0.3.0', {}),
            ('fft.bin', '0.3.0', {}), ('fft.zip', None, {}), ('fft.zip', '  \n', {}),
            ('fft.zip', 'v' * 4097, {}), ('fft.zip', '0.3.0', []),
            ('fft.zip', '0.3.0', {'padding': 'x' * 16385}),
        ):
            with self.subTest(filename=filename, version_size=len(version or '')):
                self.archive = self.archive.parent / filename
                with zipfile.ZipFile(self.archive, 'w') as archive:
                    if version is not None:
                        archive.writestr('version', version)
                    archive.writestr('instrument.json', json.dumps(metadata))
                code, result = self.execute()
                self.assertEqual(code, 1)
                self.assertEqual(result['error']['code'], 'invalid_input')
                self.assertEqual(self.events, [])

    def test_multipart_overhead_counts_toward_upload_limit(self):
        with patch.object(management, 'MAX_UPLOAD', self.archive.stat().st_size + 1):
            code, result = self.execute()
        self.assertEqual(code, 1)
        self.assertIn('multipart headers', result['error']['message'])
        self.assertEqual(self.events, [])

    def test_legacy_cli_commands_also_explain_missing_host(self):
        runner = CliRunner()
        for args in (['devices'], ['commands'], ['run'], ['upload', 'fft.zip']):
            result = runner.invoke(cli, args, env={'HOST': ''})
            self.assertEqual(result.exit_code, 2, result.output)
            self.assertIn('Specify --host or set HOST', result.output)

    def test_human_logs_use_utc_timestamps_and_json_preserves_raw_values(self):
        self.response_override = {'entries': [{'ts': 0, 'msg': 'started'}, {'msg': 'unknown time'}],
                                  'cursor': 'last', 'reset': False}
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            code = management.execute('logs', self.host)
        self.assertEqual(code, 0)
        self.assertEqual(output.getvalue(), '[1970-01-01T00:00:00.000Z] started\n[?] unknown time\n')
        code, result = self.execute('logs')
        self.assertEqual(code, 0)
        self.assertEqual(result['logs'], self.response_override)

    def test_make_and_management_command_use_consistent_timeout_defaults(self):
        for action, timeout in (('status', 10), ('logs', 10), ('deploy', 120)):
            with self.subTest(action=action):
                args = ['make', '--no-print-directory', '-n', action, f'HOST={self.host}']
                if action == 'deploy':
                    args += ['CFG=examples/red-pitaya/fft/stemlab-125-14.mk', f'INSTRUMENT_ZIP={self.archive}',
                             f'TMP={self.archive.parent}/cache', 'INSTRUMENT_FILES=']
                env = {key: value for key, value in os.environ.items() if key not in ('TIMEOUT', 'MAKEFLAGS', 'MFLAGS')}
                result = subprocess.run(args, cwd=SDK, env=env, text=True, capture_output=True, timeout=10)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertIn(f"--timeout '{timeout}'", result.stdout)
                with patch.object(management, 'Client', side_effect=management.ManagementError('fixture', 'Stop before network')) as client, \
                     contextlib.redirect_stdout(io.StringIO()):
                    self.assertEqual(management.execute(action, self.host, archive=self.archive, format='json'), 1)
                client.assert_called_once_with(self.host, timeout)

if __name__ == '__main__':
    unittest.main()
