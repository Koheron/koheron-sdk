import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class InstrumentManifestTest(unittest.TestCase):
    def test_board_architecture_and_stable_output(self):
        with tempfile.TemporaryDirectory() as directory:
            for board, architecture in [('red-pitaya', 'armhf'), ('kria-kr260', 'arm64')]:
                output = Path(directory) / board / 'instrument.json'
                command = [sys.executable, str(ROOT / 'python/koheron/instrument_manifest.py'),
                    '--board', board, '--architecture', architecture, '--sdk-version', '1.0', '--output', str(output)]
                subprocess.run(command, check=True)
                data = json.loads(output.read_text())
                self.assertEqual(data['board'], board); self.assertEqual(data['architecture'], architecture)
                self.assertEqual(data['min_runtime_api'], 1)
                previous = output.stat().st_mtime_ns
                subprocess.run(command, check=True)
                self.assertEqual(previous, output.stat().st_mtime_ns)


if __name__ == '__main__': unittest.main()
