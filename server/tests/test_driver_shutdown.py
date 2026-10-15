"""Driver dependency lifetime and acquisition shutdown with production code."""
from pathlib import Path
import tempfile
import unittest
from cpp_fixture import ROOT, NETWORK_INCLUDES, compile_cpp, run_cpp


class DriverShutdownTest(unittest.TestCase):
    def compile_run(self, sources, headers=(), flags=()):
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory)
            for name, text in headers:
                path = build / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(text)
            binary = build / 'shutdown'
            compile_cpp(self, *sources,
                flags=('-fno-exceptions', '-pthread', '-DKOHERON_SERVER_BUILD', *flags),
                includes=(build, ROOT / 'server/tests', *NETWORK_INCLUDES), output=binary)
            run_cpp(self, binary, timeout=15)

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


if __name__ == '__main__':
    unittest.main()
