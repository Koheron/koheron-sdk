"""Run with PYTHONPATH=python python3 -m unittest discover -s python/tests."""
import importlib
import tempfile
import unittest
from pathlib import Path
from unittest.mock import Mock, patch

import requests

api = importlib.import_module('koheron.koheron')


class InstrumentHttpTest(unittest.TestCase):
    def response(self, status=200, data=None):
        response = Mock()
        response.json.return_value = data
        if status >= 400:
            response.raise_for_status.side_effect = requests.HTTPError(str(status))
        return response

    @patch.object(api.requests, 'get')
    def test_status_error_propagates_before_json(self, get):
        get.return_value = self.response(500)
        with self.assertRaises(requests.HTTPError):
            api.instrument_status('example.invalid')
        get.return_value.json.assert_not_called()

    @patch.object(api.requests, 'get')
    def test_run_error_propagates(self, get):
        for status in (404, 500):
            with self.subTest(status=status):
                get.side_effect = [self.response(data={'instruments': ['fft'], 'live_instrument': 'blink'}),
                                   self.response(status)]
                with self.assertRaises(requests.HTTPError):
                    api.run_instrument('example.invalid', 'fft')

    @patch.object(api.requests, 'get')
    def test_already_running_and_explicit_restart(self, get):
        get.return_value = self.response(data={'instruments': ['fft'], 'live_instrument': 'fft'})
        api.run_instrument('example.invalid', 'fft')
        self.assertEqual(get.call_count, 1)
        get.reset_mock()
        api.run_instrument('example.invalid', 'fft', restart=True)
        self.assertEqual(get.call_count, 2)
        get.return_value.raise_for_status.assert_called()

    @patch.object(api.requests, 'get')
    def test_no_running_instrument(self, get):
        get.return_value = self.response(data={'instruments': ['fft'], 'live_instrument': None})
        with self.assertRaisesRegex(ValueError, 'No instrument is running'):
            api.run_instrument('example.invalid')
        self.assertEqual(get.call_count, 1)

    @patch.object(api.requests, 'get')
    @patch.object(api.requests, 'post')
    def test_upload_run_uses_url_encoded_archive_basename(self, post, get):
        post.return_value = self.response()
        get.return_value = self.response()
        with tempfile.TemporaryDirectory() as tmp:
            archive = Path(tmp) / 'fft #1.zip'
            archive.write_bytes(b'archive supplied to mocked upload')
            result = api.upload_instrument('example.invalid', archive, run=True)
        get.assert_called_once_with('http://example.invalid/api/instruments/run/fft%20%231')
        self.assertIs(result, get.return_value)
        get.return_value.raise_for_status.assert_called_once()

    @patch.object(api.requests, 'get')
    @patch.object(api.requests, 'post')
    def test_upload_failure_does_not_run(self, post, get):
        post.return_value = self.response(400)
        with tempfile.NamedTemporaryFile(suffix='.zip') as archive:
            with self.assertRaises(requests.HTTPError):
                api.upload_instrument('example.invalid', archive.name, run=True)
        get.assert_not_called()

    @patch.object(api.requests, 'get')
    @patch.object(api.requests, 'post')
    def test_uploaded_instrument_start_failure_propagates(self, post, get):
        post.return_value = self.response()
        get.return_value = self.response(500)
        with tempfile.NamedTemporaryFile(suffix='.zip') as archive:
            with self.assertRaises(requests.HTTPError):
                api.upload_instrument('example.invalid', archive.name, run=True)


if __name__ == '__main__':
    unittest.main()
