"""Driver dependency lifetime and acquisition shutdown with production code."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class DriverShutdownTest(unittest.TestCase):
    def compile_run(self, sources, headers=(), flags=()):
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory)
            for name, text in headers:
                path = build / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(text)
            binary = build / 'shutdown'
            command = [os.environ.get('CXX', 'g++'), '-std=c++23', '-O2', '-Wall',
                       '-Wextra', '-Werror', '-fno-exceptions', '-pthread',
                       '-DKOHERON_SERVER_BUILD', '-I', str(build), '-I', str(ROOT),
                       '-I', str(ROOT / 'server/tests'),
                       '-I', str(ROOT / 'server/external_libs'),
                       '-I', os.environ.get('EIGEN_INCLUDE_DIR', '/usr/include/eigen3')]
            command += shlex.split(os.environ.get('CXXFLAGS', ''))
            command += list(flags) + [str(ROOT / source) for source in sources]
            subprocess.run(command + ['-o', str(binary)], check=True, timeout=90)
            result = subprocess.run([str(binary)], capture_output=True, text=True, timeout=15)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_dependencies_outlive_acquisition_workers(self):
        self.compile_run(['server/tests/driver_shutdown.cpp',
                          'server/runtime/driver_manager.cpp'], headers=[
            ('drivers_list.hpp', '#pragma once\n#include "server/runtime/drivers_table.hpp"\n'
             'struct Dependency; struct Acquisition; struct Unused;\n'
             'using driver_list = std::tuple<Dependency, Acquisition, Unused>;\n'),
            ('drivers.hpp', '#include "driver_shutdown_instrument.hpp"\n'),
            ('server/context/context.hpp', '#pragma once\nclass Context {};\n')])

    def test_empty_instrument_shutdown(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / 'empty.cpp'
            source.write_text('#include "server/runtime/driver_manager.hpp"\n'
                              'int main() { rt::DriverManager manager; manager.shutdown(); }\n')
            self.compile_run([source, 'server/runtime/driver_manager.cpp'], headers=[
                ('drivers_list.hpp', '#pragma once\n#include "server/runtime/drivers_table.hpp"\n'
                 'using driver_list = std::tuple<>;\n'),
                ('drivers.hpp', '#pragma once\n'),
                ('server/context/context.hpp', '#pragma once\nclass Context {};\n')])

    def test_precision_adc_stops_before_destruction(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            discovery = root / 'spidev'
            discovery.mkdir()
            (discovery / 'spidev1.0').touch()
            # Redirect only discovery; the SPI driver and ADC worker are real.
            source = (ROOT / 'server/hardware/spi_manager.cpp').read_text()
            self.assertEqual(source.count('"/sys/class/spidev"'), 1)
            source = source.replace('"/sys/class/spidev"', f'"{discovery}"')
            spi = root / 'spi_manager.cpp'
            spi.write_text(source)
            self.compile_run(['server/tests/precision_adc_shutdown.cpp',
                              'boards/alpha250/drivers/precision-adc.cpp', spi], flags=[
                '-Wl,--wrap=open', '-Wl,--wrap=open64', '-Wl,--wrap=ioctl'])


if __name__ == '__main__':
    unittest.main()
