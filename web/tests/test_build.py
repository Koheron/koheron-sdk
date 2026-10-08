"""Regression for switching legacy web assets to older shared sources."""
import os
from pathlib import Path
import subprocess
import tempfile
import time
import unittest

WEB = Path(__file__).resolve().parents[1]


class WebBuildTests(unittest.TestCase):
    def test_config_change_replaces_older_assets_and_typescript_sources(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for name in ('legacy', 'shared'):
                (root / name).mkdir()
                (root / name / 'control.html').write_text(name)
                (root / name / 'driver.ts').write_text(name)
            (root / 'downloads.mk').write_text('')
            compiler = root / 'compiler.py'
            compiler.write_text('''import pathlib, sys
args = sys.argv[1:]
assert all(arg.endswith('.ts') for arg in args[:args.index('--outFile')])
output = pathlib.Path(args[args.index('--outFile') + 1])
content = ''.join(pathlib.Path(arg).read_text() for arg in args if arg.endswith('.ts'))
if not output.exists() or output.read_text() != content:
    output.write_text(content)
''')
            config = root / 'config.mk'
            config.write_text(f'WEB_FILES := {root}/legacy/control.html {root}/legacy/driver.ts\n')
            (root / 'Makefile').write_text(f'''CFG := {config}
WEB_PATH := {WEB}
TMP_PROJECT_PATH := {root}/out
WEB_DOWNLOADS_MK := {root}/downloads.mk
TSC := python3 {compiler}
include $(CFG)
include {WEB}/web.mk
$(TMP_WEB_PATH)/:
\tmkdir -p $@
''')
            def make(*args):
                return subprocess.run(['make', '--no-print-directory', *args], cwd=root,
                                      check=True, capture_output=True, text=True)
            make('web')
            output = root / 'out/web'
            self.assertEqual((output / 'control.html').read_text(), 'legacy')
            self.assertEqual((output / 'app.js').read_text(), 'legacy')
            stamp = time.time()
            for source in (root / 'shared').iterdir():
                os.utime(source, (stamp - 100, stamp - 100))
            for artifact in output.iterdir():
                os.utime(artifact, (stamp - 10, stamp - 10))
            config.write_text(f'WEB_FILES := {root}/shared/control.html {root}/shared/driver.ts\n')
            make('web')
            self.assertEqual((output / 'control.html').read_text(), 'shared')
            self.assertEqual((output / 'app.js').read_text(), 'shared')
            make('-q', 'web')  # An unchanged build remains up to date.
            os.utime(output / 'app.js', (stamp - 10, stamp - 10))
            config.write_text(config.read_text() + '# configuration-only change\n')
            make('web')  # Simulated incremental compiler reuses identical JS.
            make('-q', 'web')


if __name__ == '__main__':
    unittest.main()
