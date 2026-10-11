"""Bounded host commands for the V1 management API."""
import argparse
from datetime import datetime, timezone
from http.client import HTTPConnection, HTTPException, HTTPSConnection
import json
import math
from pathlib import Path
import re
import socket
import sys
from time import monotonic, sleep
from urllib.parse import quote, urlencode, urlsplit
import uuid
import zipfile

MAX_UPLOAD = 20 * 1024 * 1024


class ManagementError(Exception):
    def __init__(self, code, message, **details):
        super().__init__(f'{code}: {message}')
        self.error = {'code': str(code), 'message': str(message), **details}


class Client:
    def __init__(self, host, timeout=10):
        if not math.isfinite(timeout) or timeout <= 0:
            raise ValueError('TIMEOUT must be a finite positive number')
        host = host.strip()
        self.base_url = host if '://' in host else 'http://' + host
        parsed = urlsplit(self.base_url)
        if (parsed.scheme not in ('http', 'https') or not parsed.hostname or
                parsed.username is not None or any(char.isspace() for char in parsed.netloc) or
                parsed.path not in ('', '/') or parsed.query or parsed.fragment):
            raise ValueError('HOST must be an IP, hostname, or HTTP(S) origin, optionally with a port')
        # Validate the port before any request, including deployment uploads.
        if parsed.port == 0:
            raise ValueError('HOST port must be between 1 and 65535')
        self.base_url = self.base_url.rstrip('/')
        self.origin = parsed
        self.timeout = timeout
        self.deadline = monotonic() + timeout

    def remaining(self):
        remaining = self.deadline - monotonic()
        if remaining <= 0:
            raise ManagementError('timeout', 'Deadline exceeded; check board status before retrying a deployment')
        return remaining

    def read_response(self, response, transport):
        chunks = []
        while not response.isclosed():
            transport.settimeout(self.remaining())
            chunk = response.read1(64 * 1024)
            self.remaining()
            if not chunk:
                break
            chunks.append(chunk)
        return b''.join(chunks)

    def request(self, method, path, data=None, headers=None, json_response=True):
        connection_type = HTTPSConnection if self.origin.scheme == 'https' else HTTPConnection
        connection = connection_type(self.origin.hostname, self.origin.port, timeout=self.remaining())
        try:
            connection.connect()
            transport = connection.sock
            transport.settimeout(self.remaining())
            connection.request(method, path, body=data, headers=headers or {})
            transport.settimeout(self.remaining())
            with connection.getresponse() as response:
                content = self.read_response(response, transport)
            if response.status >= 300:
                message = content.decode('utf-8', errors='replace').strip() or response.reason
                try:
                    details = json.loads(content)
                except ValueError:
                    details = {}
                if not isinstance(details, dict):
                    details = {}
                raise ManagementError(details.pop('code', f'http_{response.status}'),
                                      details.pop('message', details.pop('error', message)), **details)
        except (TimeoutError, socket.timeout) as error:
            raise ManagementError('timeout', 'Request timed out; a board operation may still be running. Check status before retrying') from error
        except (OSError, HTTPException) as error:
            raise ManagementError('connection_failed', f'Cannot reach {self.base_url}: {error}') from error
        finally:
            connection.close()
        if not json_response:
            return content.decode('utf-8', errors='replace')
        try:
            result = json.loads(content)
            if not isinstance(result, dict):
                raise ValueError('expected a JSON object')
            return result
        except ValueError as error:
            raise ManagementError('invalid_response', f'{path} did not return a JSON object; check HOST and the V1 OS image') from error

    def status(self):
        status = self.request('GET', '/api/system/status')
        if status.get('type') != 'status' or not all(isinstance(status.get(key), dict) for key in ('instruments', 'operation', 'health')):
            raise ManagementError('runtime_mismatch', 'V1 management status is unavailable; use a V1 OS image')
        live = status['instruments'].get('live_instrument')
        service = status['health'].get('instrument_service', {})
        if (live is not None and not isinstance(live, dict)) or not isinstance(service, dict):
            raise ManagementError('invalid_response', 'V1 status contains invalid instrument or service information')
        return status

    def board(self):
        manifest = self.request('GET', '/api/system/build').get('manifest', {})
        if not isinstance(manifest, dict):
            raise ManagementError('invalid_response', 'V1 build information must contain a manifest object')
        return manifest.get('board')

    def deploy(self, archive, name=None):
        archive = Path(archive)
        name = name or archive.stem
        if name != archive.stem:
            raise ValueError('Instrument name must match the ZIP filename')
        if (archive.suffix != '.zip' or not re.fullmatch(r'[A-Za-z0-9-][A-Za-z0-9_.-]*', name)
                or name.endswith(('.', '_'))):
            raise ValueError('Use a .zip filename with letters, digits, dots, underscores or hyphens; '
                             'the instrument name must not begin or end with a dot or underscore')
        if archive.stat().st_size > MAX_UPLOAD:
            raise ValueError('Instrument ZIP exceeds the V1 API upload limit of 20 MiB')
        with zipfile.ZipFile(archive) as package:
            members = package.namelist()
            if 'version' not in members:
                raise ValueError('Instrument ZIP is missing its version file')
            for member, limit in (('version', 4096), ('instrument.json', 16384)):
                if member in members and package.getinfo(member).file_size > limit:
                    raise ValueError(f'{member} exceeds the V1 metadata limit of {limit} bytes')
            version = package.read('version').decode('utf-8').strip()
            metadata = json.loads(package.read('instrument.json')) if 'instrument.json' in members else {}
        if not version:
            raise ValueError('Instrument ZIP version must not be empty')
        if not isinstance(metadata, dict):
            raise ValueError('instrument.json must contain a JSON object')
        boundary = 'koheron-' + uuid.uuid4().hex
        content = (f'--{boundary}\r\nContent-Disposition: form-data; name="{archive.name}"; '
                   f'filename="{archive.name}"\r\nContent-Type: application/zip\r\n\r\n').encode()
        content += archive.read_bytes() + f'\r\n--{boundary}--\r\n'.encode()
        if len(content) > MAX_UPLOAD:
            raise ValueError('Instrument ZIP and multipart headers exceed the V1 upload limit of 20 MiB')
        status = self.status()
        if status['operation'].get('busy'):
            raise ManagementError('busy', 'Another instrument operation is in progress')
        board = self.board()
        if metadata.get('board') and board and metadata['board'] != board:
            raise ManagementError('board_mismatch', f'Package targets {metadata["board"]}; host reports {board}')
        self.request('POST', '/api/instruments/upload', content,
                     {'Content-Type': f'multipart/form-data; boundary={boundary}'}, json_response=False)
        escaped = quote(name, safe='')
        preflight = self.request('GET', '/api/instruments/preflight/' + escaped)
        if preflight.get('ready') is not True:
            raise ManagementError(preflight.get('code') or 'preflight_failed',
                                  preflight.get('message') or 'Instrument is not ready to activate')
        operation = self.request('POST', '/api/instruments/activate/' + escaped, data=b'')
        if operation.get('phase') == 'failed':
            raise ManagementError(operation.get('code') or 'activation_failed',
                                  operation.get('message') or 'Activation failed',
                                  rollback=operation.get('rollback', 'unknown'))
        while True:
            status = self.status()
            live = status['instruments'].get('live_instrument') or {}
            service = status['health'].get('instrument_service', {})
            operation = status['operation']
            if operation.get('instrument') == name and operation.get('phase') == 'failed':
                raise ManagementError(operation.get('code') or 'activation_failed',
                                      operation.get('message') or 'Activation failed',
                                      rollback=operation.get('rollback', 'unknown'))
            identity_matches = live.get('name') == name and live.get('version') == version
            # Health is cached briefly; a fresh live identity can precede its refresh.
            if service.get('state') == 'failed' and not identity_matches:
                raise ManagementError('service_failed', 'Instrument service failed to become ready')
            if identity_matches and service.get('state') == 'active' and not operation.get('busy'):
                return {'name': name, 'version': version, 'ready': True,
                        'warnings': preflight.get('warnings', []), 'status': status}
            sleep(min(0.25, self.remaining()))

    def logs(self, limit=100):
        path = '/api/logs/koheron?' + urlencode({'lines': limit})
        try:
            value = self.request('GET', path)
            entries = value.get('entries')
            if not isinstance(entries, list) or any(not isinstance(entry, dict) for entry in entries):
                raise ManagementError('invalid_response', 'V1 logs must contain a list of log entries')
            return value
        except ManagementError as error:
            if error.error['code'] == 'http_404' and error.error['message'].startswith('no logs for '):
                return {'entries': [], 'cursor': None, 'reset': False}
            raise


def emit(result, format='text'):
    if format == 'json':
        print(json.dumps(result, indent=2))
    elif not result['ok']:
        error = result['error']
        print(f'{error["code"]}: {error["message"]}', file=sys.stderr)
        if error.get('rollback'):
            print(f'Rollback: {error["rollback"]}', file=sys.stderr)
    elif 'deployment' in result:
        value = result['deployment']
        print(f'{value["name"]} {value["version"]} is ready on {result["host"]}')
        for warning in value['warnings']:
            print(f'Warning: {warning}', file=sys.stderr)
    elif 'status' in result:
        value = result['status']
        live = value['instruments'].get('live_instrument') or {}
        service = value['health'].get('instrument_service', {})
        identity = f'{live.get("name", "none")} {live.get("version", "")}'.strip()
        print(f'Host: {result["host"]}')
        print(f'Instrument: {identity}')
        print(f'Service: {service.get("state", "unknown")}')
        operation = value['operation']
        print(f'Operation: {operation.get("phase", "idle")} {operation.get("message", "")}'.rstrip())
    elif 'logs' in result:
        value = result['logs']
        for entry in value.get('entries', []):
            try:
                timestamp = datetime.fromtimestamp(entry['ts'] / 1_000_000, timezone.utc)
                timestamp = timestamp.isoformat(timespec='milliseconds').replace('+00:00', 'Z')
            except (KeyError, TypeError, ValueError, OverflowError, OSError):
                timestamp = '?'
            print(f'[{timestamp}] {entry.get("msg", "")}', flush=True)
        if not value.get('entries'):
            print('No server logs.')


def execute(action, host, timeout=None, format='text', archive=None, name=None, limit=100):
    try:
        if timeout is None:
            timeout = 120 if action == 'deploy' else 10
        client = Client(host, timeout)
        result = {'schema_version': 1, 'ok': True, 'host': client.base_url}
        if action == 'deploy':
            result['deployment'] = client.deploy(archive, name)
        elif action == 'status':
            result['status'] = client.status()
        else:
            if not 1 <= limit <= 2000:
                raise ValueError('LOG_LIMIT must be between 1 and 2000')
            result['logs'] = client.logs(limit)
        emit(result, format)
        return 0
    except ManagementError as error:
        emit({'schema_version': 1, 'ok': False, 'error': error.error}, format)
    except (OSError, ValueError, KeyError, zipfile.BadZipFile) as error:
        emit({'schema_version': 1, 'ok': False, 'error': {'code': 'invalid_input', 'message': str(error)}}, format)
    except KeyboardInterrupt:
        return 130
    return 1


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--host', required=True)
    parser.add_argument('--timeout', type=float, help='Seconds: 120 for deploy, 10 for status/log requests')
    parser.add_argument('--format', choices=('text', 'json'), default='text')
    commands = parser.add_subparsers(dest='action', required=True)
    deploy = commands.add_parser('deploy', help='Upload, activate, confirm readiness, then return')
    deploy.add_argument('archive', type=Path)
    deploy.add_argument('--name')
    commands.add_parser('status', help='Read board status')
    logs = commands.add_parser('logs', help='Recent server logs, across invocations')
    logs.add_argument('--limit', type=int, default=100)
    return execute(**vars(parser.parse_args(argv)))


if __name__ == '__main__':
    raise SystemExit(main())
