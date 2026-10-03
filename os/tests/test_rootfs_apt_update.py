"""Exercise the build's APT update command against an isolated local repository.

Uses private APT state and a localhost HTTP server; never installs packages.
"""
from email.utils import formatdate
import functools
import hashlib
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile
import threading
import unittest


PAYLOAD = Path(__file__).resolve().parents[1] / 'scripts/chroot_base_rootfs.sh'


class RepositoryHandler(SimpleHTTPRequestHandler):
    def do_GET(self):
        if self.server.fail:
            self.send_error(503, 'Temporary mirror failure')
        else:
            super().do_GET()

    def log_message(self, *args):
        pass


@unittest.skipUnless(shutil.which('apt-get'), 'Requires APT (use the SDK build container)')
class RootfsAptUpdateTest(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        repository = self.root / 'repo'
        repository.mkdir()
        packages = b'''Package: koheron-update-test
Version: 1
Architecture: all
Maintainer: Test <test@example.invalid>
Filename: test.deb
Size: 1
SHA256: 0000000000000000000000000000000000000000000000000000000000000000
Description: Local APT update fixture

'''
        (repository / 'Packages').write_bytes(packages)
        (repository / 'Release').write_text(
            'Origin: Koheron test\nLabel: Koheron test\n'
            + f'Date: {formatdate(usegmt=True)}\nArchitectures: amd64 armhf arm64\n'
            + 'SHA256:\n'
            + f' {hashlib.sha256(packages).hexdigest()} {len(packages)} Packages\n')
        handler = functools.partial(RepositoryHandler, directory=str(repository))
        self.server = ThreadingHTTPServer(('127.0.0.1', 0), handler)
        self.server.fail = False
        self.thread = threading.Thread(target=self.server.serve_forever,
                                       kwargs={'poll_interval': 0.02}, daemon=True)
        self.thread.start()
        self.addCleanup(self.stop_server)
        sources = self.root / 'sources.list'
        sources.write_text(f'deb [trusted=yes] http://127.0.0.1:{self.server.server_port}/ ./\n')
        (self.root / 'state/lists/partial').mkdir(parents=True)
        (self.root / 'cache/archives/partial').mkdir(parents=True)
        (self.root / 'status').touch()
        # Prevent the builder/host's repositories, hooks, caches and dpkg state
        # from participating in these tests.
        self.options = []
        for key, value in (
            ('Dir::Etc::sourcelist', sources), ('Dir::Etc::sourceparts', '-'),
            ('Dir::Etc::parts', '-'), ('Dir::Etc::main', '-'),
            ('Dir::State', self.root / 'state'), ('Dir::State::status', self.root / 'status'),
            ('Dir::Cache', self.root / 'cache'), ('Acquire::Retries', 0),
            ('Acquire::http::Timeout', 2), ('Acquire::Languages', 'none'),
        ):
            self.options.extend(['-o', f'{key}={value}'])
        self.environment = {key: value for key, value in os.environ.items()
                            if key.lower() not in ('http_proxy', 'https_proxy', 'all_proxy', 'apt_config')}
        self.environment.update({'LC_ALL': 'C', 'no_proxy': '127.0.0.1'})

    def stop_server(self):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join(timeout=5)

    def update(self, build_command=True):
        if build_command:
            commands = [shlex.split(line) for line in PAYLOAD.read_text().splitlines()
                        if line.startswith('apt-get update')]
            self.assertEqual(len(commands), 1, 'Expected one base-rootfs index refresh')
            command = commands[0]
        else:
            command = ['apt-get', 'update']
        return subprocess.run([*command, *self.options], capture_output=True, text=True,
                              env=self.environment, timeout=15)

    def test_healthy_repository_refresh_succeeds(self):
        result = self.update()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertTrue(list((self.root / 'state/lists').glob('*Packages*')))

    def test_failed_refresh_with_cached_indexes_stops_build(self):
        initial = self.update()
        self.assertEqual(initial.returncode, 0, initial.stdout + initial.stderr)
        indexes = list((self.root / 'state/lists').glob('*Packages*'))
        self.assertTrue(indexes)
        previous = {path.name: path.read_bytes() for path in indexes}
        self.server.fail = True
        # Reproduce the previous command's successful status despite errors.
        baseline = self.update(build_command=False)
        self.assertEqual(baseline.returncode, 0, baseline.stdout + baseline.stderr)
        self.assertIn('Failed to fetch', baseline.stderr)
        result = self.update()
        self.assertEqual(result.returncode, 100, result.stdout + result.stderr)
        self.assertIn('Failed to fetch', result.stderr)
        self.assertEqual({path.name: path.read_bytes() for path in indexes}, previous)


if __name__ == '__main__':
    unittest.main()
