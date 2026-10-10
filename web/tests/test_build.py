"""Regressions for web source selection and compiler dependency changes."""
import hashlib
import os
from pathlib import Path
import re
import shutil
import stat
import subprocess
import tempfile
import time
import unittest

WEB = Path(__file__).resolve().parents[1]


class WebBuildTests(unittest.TestCase):
    def test_dashboard_rebuilds_after_compiler_changes_before_web_fragment(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            web = root / 'web'
            inputs = ('compiler-inputs.mk', 'package.json', 'package-lock.json',
                      'Dockerfile.web', 'build.cjs', 'transpile.cjs')
            for name in (*inputs, 'ui-assets.mk', 'koheron.ts', 'instrument/poller.ts'):
                destination = web / name
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(WEB / name, destination)
            (root / 'downloads.mk').touch()
            events = root / 'builds'
            compiler = root / 'compiler.py'
            compiler.write_text(f'''import pathlib, sys
pathlib.Path(sys.argv[2]).write_text('compiled dashboard')
with pathlib.Path({str(events)!r}).open('a') as stream:
    stream.write('build\\n')
''')
            (root / 'Makefile').write_text(f'''\
OS_PATH := {WEB.parent}/os
WEB_PATH := {web}
TMP := {root}/out
TMP_PROJECT_PATH := {root}/instrument
WEB_DOWNLOADS_MK := {root}/downloads.mk
WEB_COMPILE := python3 {compiler}
.PHONY: FORCE
FORCE:
# Match the SDK's include order: dashboard rules precede instrument web rules.
include $(OS_PATH)/rootfs.mk
include {WEB}/web.mk
''')
            output = root / 'out/www/instruments.js'

            def make(*args):
                return subprocess.run(['make', '--no-print-directory', *args, str(output)],
                                      cwd=root, check=True, capture_output=True, text=True)

            make()
            self.assertEqual(events.read_text().splitlines(), ['build'])
            make('-q')
            for count, name in enumerate(inputs, start=2):
                with self.subTest(compiler_input=name):
                    source = web / name
                    source.write_text(source.read_text() + '\n')
                    make()
                    self.assertEqual(len(events.read_text().splitlines()), count)
                    make('-q')

    def test_management_pages_refresh_asset_versions_after_js_and_css_changes(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / 'www'
            source.mkdir()
            for path in (WEB.parent / 'os/www').iterdir():
                if path.suffix in ('.ts', '.html', '.css'):
                    shutil.copyfile(path, source / path.name)
            compiler = root / 'compiler.py'
            compiler.write_text(f"""import pathlib, sys
pathlib.Path(sys.argv[2]).write_text(pathlib.Path({str(source / 'runtime.ts')!r}).read_text())
""")
            (root / 'Makefile').write_text(f"""OS_PATH := {WEB.parent}/os
WEB_PATH := {WEB}
TMP := {root}/out
WEB_COMPILE := python3 {compiler}
.PHONY: FORCE
FORCE:
include $(OS_PATH)/rootfs.mk
""")
            output = root / 'out/www'
            pages = ('index.html', 'instrument_summary.html', 'logs_rate.html')
            assets = ('instruments.js', 'main.css', 'instrument.css', 'system.css')

            def make(*args):
                subprocess.run(['make', '--no-print-directory', f'WWW_PATH={source}',
                                *args, *(str(output / name) for name in pages)],
                               cwd=root, check=True, capture_output=True, text=True)

            def versions():
                values = {}
                for name in pages:
                    html = (output / name).read_text()
                    found = dict(re.findall(r'/koheron/([^"\'?]+)\?v=([0-9a-f]+)', html))
                    self.assertEqual(set(found), set(assets))
                    for asset in assets:
                        self.assertEqual(found[asset], hashlib.sha256((output / asset).read_bytes()).hexdigest()[:16])
                    self.assertEqual(stat.S_IMODE((output / name).stat().st_mode), 0o644)
                    values[name] = found
                return values

            make()
            initial = versions()
            make('-q')
            runtime = source / 'runtime.ts'
            runtime.write_text(runtime.read_text() + '\n// New runtime implementation\n')
            make()
            javascript_update = versions()
            for name in pages:
                self.assertNotEqual(javascript_update[name]['instruments.js'], initial[name]['instruments.js'])
                self.assertEqual(javascript_update[name]['system.css'], initial[name]['system.css'])
            css = source / 'system.css'
            css.write_text(css.read_text() + '\n/* New management layout */\n')
            make()
            stylesheet_update = versions()
            for name in pages:
                self.assertNotEqual(stylesheet_update[name]['system.css'], javascript_update[name]['system.css'])
                self.assertEqual(stylesheet_update[name]['instruments.js'], javascript_update[name]['instruments.js'])
            make('-q')

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
assert all(arg.endswith('.ts') for arg in args[2:])
output = pathlib.Path(args[1])
content = ''.join(pathlib.Path(arg).read_text() for arg in args if arg.endswith('.ts'))
output.write_text(content)
''')
            config = root / 'config.mk'
            config.write_text(f'WEB_FILES := {root}/legacy/control.html {root}/legacy/driver.ts\n')
            (root / 'Makefile').write_text(f'''CFG := {config}
WEB_PATH := {WEB}
TMP_PROJECT_PATH := {root}/out
WEB_DOWNLOADS_MK := {root}/downloads.mk
WEB_COMPILE := python3 {compiler}
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
            make('web')  # Configuration-only changes still refresh the generated script.
            make('-q', 'web')


if __name__ == '__main__':
    unittest.main()
