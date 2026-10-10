"""Regressions for web source selection and compiler dependency changes."""
import hashlib
import os
from pathlib import Path
import re
import shutil
import stat
import subprocess
import sys
import tempfile
import time
import unittest

WEB = Path(__file__).resolve().parents[1]


class WebBuildTests(unittest.TestCase):
    def test_default_compiler_refreshes_builder_and_uses_current_sources(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            web = root / 'web'
            for name in ('compiler-inputs.mk', 'package.json', 'package-lock.json',
                         'Dockerfile.web', 'build.cjs', 'transpile.cjs', 'ensure_builder.sh',
                         'plotting/package.json', 'plotting/package-lock.json',
                         'ui-assets.mk', 'koheron.ts', 'instrument/poller.ts'):
                destination = web / name
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(WEB / name, destination)
            source = root / 'source.ts'
            source.write_text('const value: number = 1;\n')
            (root / 'downloads.mk').touch()
            binary_dir = root / 'bin'
            binary_dir.mkdir()
            docker = binary_dir / 'docker'
            docker.write_text(f'''#!{sys.executable}
import hashlib, pathlib, sys
root = pathlib.Path(__file__).parent.parent
args = sys.argv[1:]
image = root / 'image-id'
if args[:2] == ['image', 'inspect']:
    if not image.exists(): raise SystemExit(1)
    print(image.read_text())
elif args[0] == 'build':
    if (root / 'fail-build').exists(): raise SystemExit(7)
    context = pathlib.Path(args[-1])
    data = b''.join(p.read_bytes() for p in sorted(context.rglob('*')) if p.is_file())
    image.write_text(hashlib.sha256(data).hexdigest())
    (root / 'image-build.cjs').write_bytes((context / 'build.cjs').read_bytes())
    (root / 'image-transpile.cjs').write_bytes((context / 'transpile.cjs').read_bytes())
    with (root / 'events').open('a') as stream: stream.write('build\\n')
elif args[0] == 'run':
    compiler = pathlib.Path(args[args.index('node') + 1])
    assert str(compiler) == '/opt/app/build.cjs', compiler
    code = (root / 'image-build.cjs').read_bytes()
    if b'CHANGED_COMPILER_MUST_FAIL' in code: raise SystemExit(8)
    output = pathlib.Path(args[args.index('--output') + 1])
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(hashlib.sha256(code + (root / 'image-transpile.cjs').read_bytes()
                                    + image.read_bytes()).hexdigest())
    with (root / 'events').open('a') as stream: stream.write('compile\\n')
else:
    raise SystemExit('unexpected docker arguments: ' + repr(args))
''')
            docker.chmod(0o755)
            (root / 'Makefile').write_text(f'''OS_PATH := {WEB.parent}/os
SERVER_PATH := {WEB.parent}/server
WEB_PATH := {web}
TMP := {root}/out
TMP_PROJECT_PATH := {root}/instrument
WEB_FILES := {source}
WEB_DOWNLOADS_MK := {root}/downloads.mk
include $(OS_PATH)/rootfs.mk
include {WEB}/web.mk
%/:
\tmkdir -p $@
''')
            outputs = [root / 'out/www/instruments.js', root / 'instrument/web/app.js']
            env = {**os.environ, 'PATH': f'{binary_dir}:{os.environ["PATH"]}',
                   'MAKEFLAGS': '', 'MFLAGS': ''}
            def make(success=True, *settings):
                result = subprocess.run(['make', '-j4', '--no-print-directory', *settings,
                    *map(str, outputs)], cwd=root, env=env, text=True,
                    capture_output=True, timeout=20)
                self.assertEqual(result.returncode == 0, success, result.stdout + result.stderr)
            def events():
                return (root / 'events').read_text().splitlines()
            make()
            self.assertEqual(events().count('build'), 1)
            self.assertEqual(events().count('compile'), 2)
            initial_events = events()
            initial_times = [p.stat().st_mtime_ns for p in outputs]
            make()
            self.assertEqual(events(), initial_events)
            self.assertEqual([p.stat().st_mtime_ns for p in outputs], initial_times)

            original = (web / 'build.cjs').read_text()
            (web / 'build.cjs').write_text("throw new Error('CHANGED_COMPILER_MUST_FAIL');\n")
            make(False)
            self.assertEqual([p.stat().st_mtime_ns for p in outputs], initial_times)
            (web / 'build.cjs').write_text(original)
            make()
            previous = [p.read_text() for p in outputs]
            for name in ('transpile.cjs', 'package-lock.json', 'plotting/package-lock.json'):
                path = web / name
                path.write_text(path.read_text() + '\n')
                before = events().count('build')
                make()
                self.assertEqual(events().count('build'), before + 1)
                self.assertNotEqual(previous, [p.read_text() for p in outputs])
                previous = [p.read_text() for p in outputs]

            stamp = root / 'out/web/.compiler-settings'
            previous_stamp = stamp.read_text()
            previous_times = [p.stat().st_mtime_ns for p in outputs]
            (web / 'transpile.cjs').write_text('// Changed again\n')
            (root / 'fail-build').touch()
            make(False)
            self.assertEqual(stamp.read_text(), previous_stamp)
            self.assertEqual([p.stat().st_mtime_ns for p in outputs], previous_times)
            self.assertEqual(list(stamp.parent.glob('.compiler-settings.tmp.*')), [])
            (root / 'fail-build').unlink()
            make()
            make(True, 'WEB_DOCKER_IMAGE=other-compiler')
            initial_events = events()
            make(True, 'WEB_DOCKER_IMAGE=other-compiler')
            self.assertEqual(events(), initial_events)
            (root / 'image-id').unlink()
            make(True, 'WEB_DOCKER_IMAGE=other-compiler')
            self.assertEqual(events().count('build'), initial_events.count('build') + 1)

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
