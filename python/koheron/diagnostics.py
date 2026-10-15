"""Shared text and JSON reporting for host diagnostics."""
import json
import os

GENERATOR_MODULES = ('yaml', 'jinja2', 'CppHeaderParser')
HOST_TEST_MODULES = GENERATOR_MODULES + ('numpy', 'requests', 'click', 'websocket')


class Reporter:
    def __init__(self, color=False, format='text'):
        self.color = color and format == 'text' and os.environ.get('NO_COLOR') is None
        self.format = format
        self.checks = []
        self.errors = 0
        self.warnings = 0

    def _line(self, level, message, remedy=None):
        record = {'status': level.lower(), 'message': message}
        if remedy:
            record['remedy'] = remedy
        self.checks.append(record)
        self.errors += level == 'ERR'
        self.warnings += level == 'WARN'
        if self.format == 'text':
            colors = {'OK': '\033[1;32m', 'WARN': '\033[1;33m', 'ERR': '\033[1;31m'}
            label = f'[{level}]'
            if self.color:
                label = colors[level] + label + '\033[0m'
            print(f'{label} {message}', flush=True)
            if remedy:
                print(f'      {remedy}', flush=True)
        return record

    def ok(self, message):
        return self._line('OK', message)

    def warn(self, message, remedy=None):
        return self._line('WARN', message, remedy)

    def err(self, message, remedy=None):
        return self._line('ERR', message, remedy)

    def finish(self, **context):
        if self.format == 'json':
            print(json.dumps({'schema_version': 1, 'ok': not self.errors,
                              **context, 'checks': self.checks}, indent=2))
        return 1 if self.errors else 0
