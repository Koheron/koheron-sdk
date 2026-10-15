"""Exercise generated RPC IDs and cached compiler commands without compiling."""
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest


SDK = Path(__file__).resolve().parents[2]


class ServerBuildMakeTest(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.output = self.root / 'out/server'
        self.generator = self.root / 'generator.py'
        self.generator.write_text('''import pathlib, subprocess, sys
with pathlib.Path(__file__).with_name('renders').open('a') as stream:
    stream.write(sys.argv[2] + '\\n')
raise SystemExit(subprocess.call([sys.executable, sys.argv[1], *sys.argv[2:]]))
''')
        # A separate generator input lets us test edits without changing SDK files.
        self.generator_input = self.root / 'generator-input.py'
        self.generator_input.touch()
        (self.root / 'memory.yml').write_text('{}\n')
        for name in ('Alpha', 'Beta', 'Gamma'):
            path = self.root / f'{name}.hpp'
            path.write_text(f'class {name} {{ public: void ping() {{}} }};\n')
            os.utime(path, (100, 100))
        self.compiler = self.root / 'compiler.py'
        self.compiler.write_text('''import json, pathlib, sys
args = sys.argv[1:]
output = pathlib.Path(args[args.index('-o') + 1])
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(json.dumps(args))
with pathlib.Path(__file__).with_name('compiles').open('a') as stream:
    stream.write(str(output) + '\\n')
''')
        (self.root / 'probe.cpp').write_text('int value() { return FEATURE; }\n')
        self.harness = self.root / 'Makefile'
        self.harness.write_text(f'''SHELL := /bin/bash
.SHELLFLAGS := -eu -o pipefail -c
.DELETE_ON_ERROR:
SDK_PATH := {SDK}
SDK_FULL_PATH := {SDK}
export SDK_PATH
OS_PATH := {SDK}/os
SERVER_PATH := {SDK}/server
PROJECT_PATH := {self.root}
NAME := probe
TMP_PROJECT_PATH := {self.root}/out
SERVER := $(TMP_PROJECT_PATH)/serverd
MEMORY_YML := {self.root}/memory.yml
MAKE_PY := {sys.executable} {self.generator} {SDK}/make.py
GCC_ARCH := arm-linux-gnueabihf
GCC_VERSION := 15
N_CPUS := 1
include {SDK}/os/compiler-settings.mk
include {SDK}/server/server.mk
''')

    def make(self, *args):
        result = subprocess.run(['make', '--no-print-directory', '-j4',
                                 '-f', str(self.harness), *map(str, args)],
                                cwd=self.root, text=True, capture_output=True,
                                env={**os.environ, 'MAKEFLAGS': '', 'MFLAGS': ''}, timeout=20)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return result.stdout

    def generate(self, names):
        headers = [self.root / f'{name}.hpp' for name in names]
        targets = [self.output / name for name in ('drivers_list.hpp', 'interface_drivers.hpp',
                   'drivers_json.hpp', 'drivers.hpp', 'operations.hpp', 'memory.hpp')]
        targets += [self.output / f'interface_{name}.hpp' for name in names]
        self.make('DRIVERS=' + ' '.join(map(str, headers)),
                  f'MAKE_PY_SOURCES={self.generator_input}', *targets)
        for expected_id, name in enumerate(names, 2):
            text = (self.output / f'interface_{name}.hpp').read_text()
            self.assertRegex(text, rf'constexpr int {name}Id = {expected_id};')
        adapters = re.findall(r'(\w+)Adapter', (self.output / 'drivers_json.hpp').read_text())
        self.assertEqual(adapters, names)
        return {path.name: path.stat().st_mtime_ns for path in targets}

    def test_reordering_inserting_and_removing_drivers_refreshes_all_ids(self):
        initial = self.generate(['Alpha', 'Beta'])
        self.assertEqual(self.generate(['Alpha', 'Beta']), initial)
        self.generate(['Beta', 'Alpha'])
        self.generate(['Beta', 'Gamma', 'Alpha'])  # Gamma is older than the cache.
        (self.root / 'Alpha.hpp').unlink()
        self.generate(['Beta', 'Gamma'])
        for name in ('drivers.hpp', 'operations.hpp', 'drivers_json.hpp', 'drivers_list.hpp'):
            self.assertNotIn('Alpha', (self.output / name).read_text())
        unchanged = self.generate(['Beta', 'Gamma'])
        self.assertEqual(self.generate(['Beta', 'Gamma']), unchanged)

    def test_python_generator_changes_refresh_cached_interfaces_and_memory(self):
        initial = self.generate(['Alpha', 'Beta'])
        events = (self.root / 'renders').read_text()
        self.generate(['Alpha', 'Beta'])
        self.assertEqual((self.root / 'renders').read_text(), events)
        self.generator_input.write_text('# Changed generator\n')
        updated = self.generate(['Alpha', 'Beta'])
        self.assertNotEqual(initial['interface_Beta.hpp'], updated['interface_Beta.hpp'])
        renders = (self.root / 'renders').read_text().splitlines()[len(events.splitlines()):]
        self.assertIn('--memory_hpp', renders)
        self.assertIn('--render_interface', renders)
        self.assertIn('--render_template', renders)
        # The real generator keeps an identical memory.hpp timestamp stable.
        self.assertEqual(initial['memory.hpp'], updated['memory.hpp'])

    def test_complete_flags_and_link_membership_invalidate_cached_outputs(self):
        server = self.root / 'out/serverd'
        common = ['GEN_HEADERS=', 'SERVER_OBJ=', 'SERVER_LIB_OBJ=', 'HARDWARE_OBJ=',
                  f'SERVER_CCXX={sys.executable} {self.compiler}',
                  f'PCH_CXX={sys.executable} {self.compiler}']
        def build(flags, drivers='probe.cpp'):
            self.make(*common, f'SERVER_EXTRA_CCXXFLAGS={flags}', f'DRIVERS={drivers}', server)
        build('-DFEATURE=1')
        obj = self.output / 'probe.o'
        self.assertIn('-DFEATURE=1', json.loads(obj.read_text()))
        initial = (self.root / 'compiles').read_text()
        build('-DFEATURE=1')
        self.assertEqual((self.root / 'compiles').read_text(), initial)
        pch_time = (self.root / 'out/pch/pch.hpp.gch').stat().st_mtime_ns
        build('-DFEATURE=2')
        self.assertIn('-DFEATURE=2', json.loads(obj.read_text()))
        self.assertEqual((self.root / 'out/pch/pch.hpp.gch').stat().st_mtime_ns, pch_time)
        build("-DFEATURE=2 -DTEXT='literal'")  # Quotes must survive settings publication.
        before = (self.root / 'compiles').read_text()
        build("-DFEATURE=2 -DTEXT='literal'", drivers='')
        self.assertNotIn(str(obj), json.loads(server.read_text()))
        self.assertEqual((self.root / 'compiles').read_text().count(str(obj)), before.count(str(obj)))

    def test_metadata_dump_runs_from_absolute_and_relative_artifact_paths(self):
        compiler = self.root / 'metadata-compiler.py'
        compiler.write_text('''import pathlib, sys
args = sys.argv[1:]
output = pathlib.Path(args[args.index('-o') + 1])
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text('#!/bin/sh\\nprintf \\'%s\\' \\'{"instrument":"probe"}\\'\\n')
output.chmod(0o755)
''')
        for project in (str(self.root / 'absolute'), 'relative'):
            with self.subTest(project=project):
                target = Path(project) / 'server/drivers.json'
                self.make(f'TMP_PROJECT_PATH={project}', 'GEN_HEADERS=', 'DOCKER=',
                          f'JSON_CXX={sys.executable} {compiler}', target)
                self.assertEqual(json.loads((self.root / target).read_text()),
                                 {'instrument': 'probe'})

    def test_metadata_inherits_header_flags_without_cross_target_options(self):
        target = self.output / 'drivers_json_dump'
        def build(extra):
            self.make('GEN_HEADERS=', 'DOCKER=',
                      f'JSON_CXX={sys.executable} {self.compiler}',
                      f'SERVER_EXTRA_CCXXFLAGS={extra}', target)
            return json.loads(target.read_text())
        for header_flags, expected in (
                ('-I/external/include -DEXTERNAL_FEATURE=1 -UOLD_FEATURE',
                 ['-I/external/include', '-DEXTERNAL_FEATURE=1', '-UOLD_FEATURE']),
                ('-I "/external/include dir" -D EXTERNAL_FEATURE=1 -U OLD_FEATURE',
                 ['-I', '/external/include dir', '-D', 'EXTERNAL_FEATURE=1', '-U', 'OLD_FEATURE']),
                ('''-I"/external/it's include" -DLABEL='"hello world"' -DEXTERNAL_FEATURE=1 -UOLD_FEATURE''',
                 ["-I/external/it's include", '-DLABEL="hello world"',
                  '-DEXTERNAL_FEATURE=1', '-UOLD_FEATURE'])):
            with self.subTest(flags=header_flags):
                flags = header_flags + ' -mfpu=neon -mcpu=cortex-a9'
                args = build(flags)
                start = args.index(expected[0])
                self.assertEqual(args[start:start + len(expected)], expected)
                self.assertIn('-DINSTRUMENT_NAME="probe"', args)
                self.assertNotIn('-mfpu=neon', args)
                self.assertNotIn('-mcpu=cortex-a9', args)
                initial = (self.root / 'compiles').read_text()
                build(flags)
                self.assertEqual((self.root / 'compiles').read_text(), initial)
                args = build(flags.replace('FEATURE=1', 'FEATURE=2'))
                self.assertTrue(any('FEATURE=2' in arg for arg in args))
                self.assertNotEqual((self.root / 'compiles').read_text(), initial)

    def test_custom_app_driver_selection_refreshes_generated_type_table(self):
        app_mk = SDK / 'examples/alpha250/fft/app/app.mk'
        self.harness.write_text(self.harness.read_text().replace(
            f'include {SDK}/server/server.mk', f'include {app_mk}'))
        self.make('-n', 'clean_server')
        self.assertFalse(self.output.parent.exists(), 'reading app.mk created build artifacts')
        targets = [self.output / name for name in ('drivers.hpp', 'drivers_list.hpp')]

        def generate(names):
            self.make('DRIVERS=' + ' '.join(str(self.root / f'{name}.hpp') for name in names),
                      f'MAKE_PY_SOURCES={self.generator_input}', *targets)
            includes, table = [path.read_text() for path in targets]
            for name in names:
                self.assertIn(f'{name}.hpp', includes)
            self.assertEqual(re.findall(r'class (\w+);', table), names)
            return [path.stat().st_mtime_ns for path in targets]

        initial = generate(['Alpha', 'Beta'])
        self.assertEqual(generate(['Alpha', 'Beta']), initial)
        generate(['Beta', 'Alpha'])
        (self.root / 'Alpha.hpp').unlink()
        generate(['Beta'])
        for path in targets:
            self.assertNotIn('Alpha', path.read_text())


class AlternateServerBuildMakeTest(unittest.TestCase):
    def test_flags_and_source_removal_refresh_cached_server(self):
        for makefile in ('server/serverless.mk', 'server/server-bare.mk',
                         'examples/alpha250/fft/app/app.mk'):
            with self.subTest(makefile=makefile), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                app = root / 'app'
                app.mkdir()
                for name in ('main', 'extra'):
                    (app / f'{name}.cpp').write_text(f'int {name}() {{ return 0; }}\n')
                (root / 'memory.yml').write_text('memory: []\n')
                (root / 'requirements.stamp').touch()
                compiler = root / 'compiler.py'
                compiler.write_text('''import json, pathlib, sys
args = sys.argv[1:]
output = pathlib.Path(args[args.index('-o') + 1])
if '-c' in args:
    # Model a real compiler: it needs its output directory and generated headers
    # to exist, and emits header dependencies for subsequent cached builds.
    header = pathlib.Path(__file__).with_name('out') / 'server/memory.hpp'
    assert header.is_file(), 'compiled before memory.hpp was generated'
    output.with_suffix('.d').write_text(f'{output}: {header}\\n')
output.write_text(json.dumps(args))
with pathlib.Path(__file__).with_name('events').open('a') as stream:
    stream.write(('compile' if '-c' in args else 'link') + '\\n')
''')
                generator = root / 'generator.py'
                generator.write_text('''import pathlib, sys, time
time.sleep(0.05)
path = pathlib.Path(sys.argv[2])
path.write_text('// generated\\n')
''')
                harness = root / 'Makefile'
                server = root / 'out/serverd'
                harness.write_text(f'''SHELL := /bin/bash
.SHELLFLAGS := -eu -o pipefail -c
.DELETE_ON_ERROR:
SDK_PATH := {SDK}
SDK_FULL_PATH := {SDK}
SERVER_PATH := {SDK}/server
PROJECT_PATH := {app}
TMP_PROJECT_PATH := {root}/out
SERVER := {server}
MEMORY_YML := {root}/memory.yml
PYTHON_REQUIREMENTS_STAMP := {root}/requirements.stamp
MAKE_PY := {sys.executable} {generator}
MAKE_PY_SOURCES := {generator}
GCC_ARCH := arm-linux-gnueabihf
GCC_VERSION := 15
N_CPUS := 1
DOCKER := {sys.executable} {compiler}
NAME := original
%/:
\tmkdir -p $@
include {SDK}/os/compiler-settings.mk
include {SDK}/{makefile}
''')

                def build(name, sources, flags=''):
                    selection = ('DRIVERS=' + ' '.join(map(str, sources[1:]))) if makefile.endswith('app.mk') else (
                        'SERVERLESS_CPP_SRCS=' + ' '.join(map(str, sources)))
                    result = subprocess.run(
                        ['make', '--no-print-directory', '-j4', '-f', str(harness),
                         f'NAME={name}', selection, f'SERVER_EXTRA_CCXXFLAGS={flags}',
                         f'APP_PATH={app}', 'SERVER_LIB_OBJ=', 'SERVER_NET_OBJ=', 'HARDWARE_OBJ=',
                         f'MAKE_PY_SOURCES={generator}',
                         str(server)], cwd=root, text=True, capture_output=True, timeout=20)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

                sources = [app / 'main.cpp', app / 'extra.cpp']
                build('original', sources)
                flags = json.loads((root / 'out/server/main.o').read_text())
                self.assertIn('-std=c++23', flags)
                self.assertEqual('-Werror' in flags, makefile.endswith('app.mk'))
                self.assertNotIn('-fno-exceptions', flags)
                initial_events = (root / 'events').read_text()
                build('original', sources)
                self.assertEqual((root / 'events').read_text(), initial_events)

                build('renamed', sources)
                self.assertIn('renamed', (root / 'out/server/main.o').read_text())
                renamed_events = (root / 'events').read_text()
                self.assertGreater(renamed_events.count('compile'), initial_events.count('compile'))

                build('renamed', sources, flags='-DFEATURE=1')
                self.assertIn('-DFEATURE=1', json.loads((root / 'out/server/main.o').read_text()))
                renamed_events = (root / 'events').read_text()

                build('renamed', sources[:1], flags='-DFEATURE=1')
                self.assertNotIn('extra.o', server.read_text())
                self.assertEqual((root / 'events').read_text().count('compile'),
                                 renamed_events.count('compile'))
                self.assertEqual((root / 'events').read_text().count('link'),
                                 renamed_events.count('link') + 1)

                # A shared parser/generator edit must regenerate the header and
                # rebuild its consumers even when memory.yml is unchanged.
                previous = (root / 'events').read_text()
                generator.write_text(generator.read_text() + '\n# changed generator input\n')
                build('renamed', sources[:1], flags='-DFEATURE=1')
                self.assertGreater((root / 'events').read_text().count('compile'), previous.count('compile'))


if __name__ == '__main__':
    unittest.main()
