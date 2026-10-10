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


if __name__ == '__main__':
    unittest.main()
