"""Public downloaded assets must remain readable when packaged for nginx."""
import hashlib
from pathlib import Path
import stat
import subprocess
import tempfile
import unittest


SCRIPT = Path(__file__).resolve().parents[1] / 'scripts/download_verified.sh'


class VerifiedDownloadPermissionsTest(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.source = self.root / 'source.js'
        self.source.write_bytes(b'window.libraryLoaded = true;\n')
        self.destination = self.root / 'assets/library.js'
        self.checksums = self.root / 'checksums'
        self.checksums.write_text(hashlib.sha256(self.source.read_bytes()).hexdigest() + '  library.js\n')

    def download(self, url=None):
        return subprocess.run(['bash', str(SCRIPT), str(self.checksums), str(self.destination),
                               url or self.source.as_uri()], capture_output=True, text=True,
                              timeout=5, umask=0o077)

    def assert_readable(self, result):
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(self.destination.read_bytes(), self.source.read_bytes())
        self.assertEqual(stat.S_IMODE(self.destination.stat().st_mode), 0o644)

    def test_fresh_download_is_readable_under_private_umask(self):
        self.assert_readable(self.download())

    def test_verified_private_cache_is_repaired_without_downloading(self):
        self.destination.parent.mkdir()
        self.destination.write_bytes(self.source.read_bytes())
        self.destination.chmod(0o600)
        previous_mtime = self.destination.stat().st_mtime_ns
        self.assert_readable(self.download('file:///nonexistent-verified-download-fixture'))
        self.assertEqual(self.destination.stat().st_mtime_ns, previous_mtime)

    def test_bad_checksum_keeps_existing_file_and_permissions(self):
        self.destination.parent.mkdir()
        self.destination.write_bytes(b'previous asset')
        self.destination.chmod(0o600)
        self.source.write_bytes(b'incorrect replacement')
        result = self.download()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('SHA-256 mismatch', result.stderr)
        self.assertEqual(self.destination.read_bytes(), b'previous asset')
        self.assertEqual(stat.S_IMODE(self.destination.stat().st_mode), 0o600)
        self.assertEqual(list(self.destination.parent.glob('*.download.*')), [])


if __name__ == '__main__':
    unittest.main()
