"""Read log batches over WebSocket, resuming through HTTP when unavailable."""
from contextlib import closing
import json
import math
import sys
import time
from urllib.parse import urlencode

import requests
import websocket

REQUEST_TIMEOUT = 10


def log_batches(base_url, cursor=None, poll_interval=1.0):
    if not math.isfinite(poll_interval) or poll_interval <= 0:
        raise ValueError('poll_interval must be a finite positive number')
    # The live stream covers the same service as the legacy instrument route.
    root = base_url.removesuffix('/instrument')
    events_url = root.replace('http:', 'ws:', 1).replace('https:', 'wss:', 1) + '/events'
    retry_at = 0
    fallback_reported = False

    def accept(data):
        nonlocal cursor
        if (not isinstance(data, dict) or not isinstance(data.get('entries'), list)
                or any(not isinstance(entry, dict) or not isinstance(entry.get('msg'), str)
                       for entry in data['entries'])
                or (data.get('cursor') is not None and not isinstance(data['cursor'], str))
                or (data['entries'] and not data.get('cursor'))):
            raise ValueError('Invalid log batch')
        if data.get('reset'):
            print('[log-stream] Log history reset; resuming from recent entries.', file=sys.stderr)
            cursor = None
        elif data.get('cursor') == cursor:
            data = {**data, 'entries': []}  # Heartbeats and replayed batches.
        if data.get('cursor'):
            cursor = data['cursor']
        return data

    with requests.Session() as session:
        while True:
            if time.monotonic() >= retry_at:
                try:
                    query = '?' + urlencode({'cursor': cursor}) if cursor else ''
                    with closing(websocket.create_connection(events_url + query, timeout=REQUEST_TIMEOUT)) as connection:
                        fallback_reported = False
                        while True:
                            message = connection.recv()
                            if not message:
                                raise websocket.WebSocketConnectionClosedException('Log stream closed')
                            if not isinstance(message, str) or len(message) > 65536:
                                raise ValueError('Invalid log frame')
                            yield accept(json.loads(message))
                except (websocket.WebSocketException, OSError, ValueError) as error:
                    if not fallback_reported:
                        print(f'[log-stream] WebSocket unavailable; using HTTP: {error}', file=sys.stderr)
                        fallback_reported = True
                    unsupported = getattr(error, 'status_code', None) in (404, 405, 426)
                    retry_at = math.inf if unsupported else time.monotonic() + 5
            try:
                params = {'cursor': cursor} if cursor else {}
                response = session.get(base_url + '/incr', params=params, timeout=REQUEST_TIMEOUT)
                response.raise_for_status()
                yield accept(response.json())
            except (requests.RequestException, ValueError) as error:
                print(f'[log-stream] {type(error).__name__}: {error}', file=sys.stderr)
            time.sleep(poll_interval)
