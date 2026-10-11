"""Log transport contracts: cursor recovery, fallback and interruption."""
import importlib
import io
import json
import unittest
from unittest.mock import Mock, patch

from koheron import instrument_runner, log_stream

api = importlib.import_module('koheron.koheron')
BASE = 'http://board/api/logs/koheron/instrument'


def batch(cursor, message=None, reset=False):
    return {'type': 'logs', 'cursor': cursor, 'reset': reset,
            'entries': [] if message is None else [{'ts': 100, 'msg': message}]}


class LogStreamTest(unittest.TestCase):
    def setUp(self):
        self.connection = Mock()
        self.connect = self.mock(log_stream.websocket, 'create_connection')
        self.connect.return_value = self.connection
        self.session_factory = self.mock(log_stream.requests, 'Session')
        self.session = self.session_factory.return_value.__enter__.return_value
        self.mock(log_stream.time, 'sleep')
        self.clock = self.mock(log_stream.time, 'monotonic')
        self.clock.return_value = 0
        self.errors = io.StringIO()
        redirect = patch.object(log_stream.sys, 'stderr', self.errors)
        redirect.start(); self.addCleanup(redirect.stop)
        self.reads = []

        def get(url, **options):
            self.reads.append((url, dict(options['params'])))
            return self.response

        self.response = Mock()
        self.session.get.side_effect = get

    def mock(self, target, name):
        replacement = patch.object(target, name)
        result = replacement.start()
        self.addCleanup(replacement.stop)
        return result

    def reader(self, cursor=None, base=BASE):
        reader = log_stream.log_batches(base, cursor)
        self.addCleanup(reader.close)
        return reader

    def test_websocket_encodes_cursor_and_does_not_poll_or_repeat_batches(self):
        value = batch('next', 'ready')
        self.connection.recv.side_effect = [json.dumps(value), json.dumps(value),
                                           json.dumps(batch('next'))]
        reader = self.reader('s=seed;i=1')
        self.assertEqual(next(reader), value)
        self.assertEqual(next(reader)['entries'], [])
        self.assertEqual(next(reader)['entries'], [])
        self.connect.assert_called_once_with(
            'ws://board/api/logs/koheron/events?cursor=s%3Dseed%3Bi%3D1', timeout=10)
        self.session.get.assert_not_called()
        reader.close()
        self.connection.close.assert_called_once()
        self.session_factory.return_value.__exit__.assert_called_once()

    def test_drop_reads_http_from_last_cursor_and_reconnects_after_fallback(self):
        second = Mock()
        self.connect.side_effect = [self.connection, second]
        self.connection.recv.side_effect = [json.dumps(batch('one', 'first')), '']
        self.response.json.return_value = batch('two', 'second')
        second.recv.return_value = json.dumps(batch('three', 'third'))
        self.clock.side_effect = [0, 0, 6]
        reader = self.reader('seed')
        self.assertEqual([next(reader)['entries'][0]['msg'] for _ in range(3)],
                         ['first', 'second', 'third'])
        self.assertEqual(self.reads, [(BASE + '/incr', {'cursor': 'one'})])
        self.assertEqual(self.connect.call_args.args[0],
                         'ws://board/api/logs/koheron/events?cursor=two')
        self.connection.close.assert_called_once()
        self.assertIn('using HTTP', self.errors.getvalue())

    def test_unsupported_endpoint_keeps_legacy_http_without_retrying_websocket(self):
        self.connect.side_effect = log_stream.websocket.WebSocketBadStatusException('Not found', 404)
        self.response.json.side_effect = [batch('one', 'first'), batch('two', 'second')]
        reader = self.reader('seed')
        self.assertEqual(next(reader)['cursor'], 'one')
        self.assertEqual(next(reader)['cursor'], 'two')
        self.connect.assert_called_once()
        self.assertEqual(self.reads, [(BASE + '/incr', {'cursor': 'seed'}),
                                     (BASE + '/incr', {'cursor': 'one'})])

    def test_malformed_websocket_batch_falls_back_without_advancing_cursor(self):
        for payload in ('invalid json', '[]', '{"entries": "bad"}',
                        json.dumps(batch(None, 'missing cursor'))):
            with self.subTest(payload=payload):
                self.reads.clear()
                self.connection.recv.return_value = payload
                self.response.json.return_value = batch('next', 'recovered')
                reader = self.reader('seed')
                self.assertEqual(next(reader)['entries'][0]['msg'], 'recovered')
                self.assertEqual(self.reads, [(BASE + '/incr', {'cursor': 'seed'})])
                reader.close()

    def test_reset_clears_expired_cursor_and_allows_recent_history(self):
        self.connection.recv.side_effect = [json.dumps(batch(None, reset=True)),
                                           json.dumps(batch('new', 'recent', reset=True))]
        reader = self.reader('expired')
        self.assertTrue(next(reader)['reset'])
        self.assertEqual(next(reader)['entries'][0]['msg'], 'recent')
        self.assertIn('Log history reset', self.errors.getvalue())

    def test_https_uses_secure_websocket(self):
        self.connection.recv.return_value = json.dumps(batch('one', 'ready'))
        self.assertEqual(next(self.reader(base=BASE.replace('http:', 'https:')))['cursor'], 'one')
        self.connect.assert_called_once_with('wss://board/api/logs/koheron/events', timeout=10)

    def test_run_ctrl_c_closes_stream_and_preserves_terminal_formatting(self):
        self.connection.recv.side_effect = [json.dumps(batch('one', 'WARNING: late')), KeyboardInterrupt]
        output = io.StringIO()
        with patch.object(instrument_runner, 'upload_instrument') as upload, \
             patch.object(instrument_runner, 'run_instrument') as run, \
             patch.object(instrument_runner, 'instrument_logs_bookmark', return_value='seed'), \
             patch.object(instrument_runner.sys, 'stdout', output):
            self.assertEqual(instrument_runner.main(['--host', 'board', '--name', 'fft', 'fft.zip']), 0)
        upload.assert_called_once_with('board', 'fft.zip', run=False)
        run.assert_called_once_with('board', 'fft', restart=True)
        self.assertIn(api.BLUE + '[0.000000s]', output.getvalue())
        self.assertIn(api.YELLOW + 'late' + api.RESET, output.getvalue())
        self.assertIn('Log streaming interrupted.', output.getvalue())
        self.connection.close.assert_called_once()
        self.session_factory.return_value.__exit__.assert_called_once()
        self.session.get.assert_not_called()

    def test_invalid_poll_interval_fails_before_network(self):
        for interval in (0, -1, float('nan'), float('inf')):
            with self.subTest(interval=interval):
                with self.assertRaisesRegex(ValueError, 'finite positive'):
                    next(log_stream.log_batches(BASE, poll_interval=interval))
        self.connect.assert_not_called()
        self.session_factory.assert_not_called()


if __name__ == '__main__':
    unittest.main()
