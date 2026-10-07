"""Query fake Xilinx tools without requiring a vendor installation."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest


REPORTER = Path(__file__).resolve().parents[1] / 'scripts/toolchain_versions.sh'


class ToolchainManifestTest(unittest.TestCase):
    def test_versions_and_unavailable_values(self):
        cases = (
            ('2025.1', '1234567', 0, '2025.1', '1234567'),
            ('2026.1.1', '7654321', 0, '2026.1.1', '7654321'),
            ('2026.1', '', 0, '2026.1', 'unknown'),
            ('unexpected', '1234567', 0, 'unknown', 'unknown'),
            ('2026.1', '1234567', 7, 'unknown', 'unknown'),
            ('missing', '', 0, 'unknown', 'unknown'),
        )
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            paths = [root / 'selected Vivado', root / 'selected Vitis']
            for release, build, status, expected_release, expected_build in cases:
                with self.subTest(release=release, status=status):
                    for tool, path, option in zip(('vivado', 'vitis'), paths,
                                                  ('-version', '--version')):
                        executable = path / 'bin' / tool
                        executable.parent.mkdir(parents=True, exist_ok=True)
                        executable.unlink(missing_ok=True)
                        if release == 'missing':
                            continue
                        executable.write_text(f'''#!/usr/bin/python3
import os, sys
assert sys.argv[1:] == [{option!r}] and 'BASH_ENV' not in os.environ
print('****** {tool.title()} v{release} (64-bit)', file=sys.stderr)
print('  **** SW Build {build} on 2026-06-16')
sys.exit({status})
''')
                        executable.chmod(0o755)
                    result = subprocess.run(
                        ['bash', str(REPORTER), *map(str, paths)], text=True,
                        capture_output=True, timeout=5,
                        env={**os.environ, 'BASH_ENV': '/unavailable-sdk-trap'})
                    self.assertEqual(result.returncode, 0, result.stderr)
                    expected = ''.join(f'{tool}={expected_release}\n{tool}_build={expected_build}\n'
                                       for tool in ('vivado', 'vitis'))
                    self.assertEqual(result.stdout, expected)
                    self.assertEqual('[WARN]' in result.stderr, 'unknown' in expected)
