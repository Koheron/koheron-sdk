"""Device failures and move-only IRQ callbacks using wrapped I/O and a raw PTY."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest
ROOT = Path(__file__).resolve().parents[2]

class DeviceInitializationTest(unittest.TestCase):
    def compile_run(self, sources, includes=(), flags=(), args=()):
        with tempfile.TemporaryDirectory() as directory:
            binary = Path(directory) / 'test'
            command = [os.environ.get('CXX', 'g++'), '-std=c++23', '-O2', '-Wall',
                       '-Wextra', '-Werror', '-fno-exceptions', '-pthread']
            for path in [*includes, ROOT]:
                command += ['-I', str(path)]
            command += shlex.split(os.environ.get('CXXFLAGS', ''))
            command += list(flags) + [str(ROOT / s) for s in sources]
            subprocess.run(command + ['-o', str(binary)], check=True, timeout=90)
            result = subprocess.run([str(binary), *map(str, args)], capture_output=True,
                                    text=True, timeout=10)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            return result.stdout + result.stderr

    def test_i2c_path_lifetime_and_retry(self):
        output = self.compile_run(['server/tests/i2c_initialization.cpp',
                                   'server/hardware/i2c_manager.cpp'], flags=[
            '-Wl,--wrap=open', '-Wl,--wrap=open64', '-Wl,--wrap=opendir',
            '-Wl,--wrap=readdir', '-Wl,--wrap=readdir64', '-Wl,--wrap=closedir'])
        self.assertIn('Permission denied', output)

    def test_move_only_uio_callback_lifetime(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root/'sys/uio0/maps/map0').mkdir(parents=True)
            (root/'sys/uio0/maps/map0/addr').write_text('0x1000\n')
            (root/'dev').mkdir()
            # Redirect only fixed discovery roots; use the production UIO code.
            source = (ROOT/'server/drivers/uio.hpp').read_text()
            self.assertEqual(source.count('"/sys/class/uio"'), 1)
            self.assertEqual(source.count('fs::path("/dev")'), 1)
            source = source.replace('"/sys/class/uio"', '"'+str(root/'sys')+'"')
            source = source.replace('fs::path("/dev")', 'fs::path("'+str(root/'dev')+'")')
            header = root/'server/drivers/uio.hpp'
            header.parent.mkdir(parents=True)
            header.write_text(source)
            (root/'memory.hpp').write_text('''#pragma once
#include <array>
#include <tuple>
#include <string_view>
constexpr auto memory_array = std::array{std::tuple{uintptr_t{0x1000}, uint32_t{4096},
uint32_t{3}, uint32_t{1}, std::string_view{"uio"}, std::string_view{"test"}}};
''')
            self.compile_run(['server/tests/uio_callbacks.cpp'], includes=[root], args=[root])

if __name__ == '__main__':
    unittest.main()
