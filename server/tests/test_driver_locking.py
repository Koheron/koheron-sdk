"""Driver locking regressions using production dispatch and real local sockets."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class DriverLockingTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        build = tempfile.TemporaryDirectory()
        cls.addClassCleanup(build.cleanup)
        build = Path(build.name)
        (build / 'drivers_list.hpp').write_text('''
#pragma once
#include "server/runtime/drivers_table.hpp"
struct LockingInstrument;
using driver_list = std::tuple<LockingInstrument>;
''')
        (build / 'drivers.hpp').write_text('#include "driver_locking_instrument.hpp"\n')
        methods = ('set', 'get', 'set_vector', 'get_vector', 'get_span', 'get_array',
                   'get_fixed_span', 'get_views', 'busy')
        ops = ','.join(f'Op<&LockingInstrument::{method}>' for method in methods)
        (build / 'interface_drivers.hpp').write_text(f'''
#include "server/executor/driver_adapter.hpp"
#include "drivers.hpp"
namespace koheron {{
using TestAdapter = DriverAdapter<2, LockingInstrument, {ops}>;
template<> class Driver<2> : public TestAdapter {{ public: using TestAdapter::TestAdapter; }};
}}
''')
        (build / 'drivers_json.hpp').write_text('''
#include <string>
namespace koheron { inline std::string build_drivers_json() { return "[]"; } }
''')
        # Only the hardware Context is stubbed; dispatch, services and all I/O are real.
        context = build / 'server/context/context.hpp'
        context.parent.mkdir(parents=True)
        context.write_text('#pragma once\nclass Context {};\n')
        cls.binary = build / 'driver-locking'
        sources = ('executor/executor.cpp', 'runtime/driver_manager.cpp',
                   'runtime/runtime_executor.cpp', 'network/session.cpp',
                   'network/websocket.cpp', 'network/sha1.cpp', 'network/base64.cpp',
                   'utilities/rate_tracker.cpp', 'tests/driver_locking.cpp')
        command = [os.environ.get('CXX', 'g++'), '-std=c++20', '-O2', '-Wall',
                   '-Wextra', '-Werror', '-pthread', '-DKOHERON_SERVER_BUILD',
                   '-I', str(build), '-I', str(ROOT), '-I', str(ROOT / 'server/tests'),
                   '-I', str(ROOT / 'server/external_libs'),
                   '-I', os.environ.get('EIGEN_INCLUDE_DIR', '/usr/include/eigen3')]
        command += shlex.split(os.environ.get('CXXFLAGS', ''))
        command += [str(ROOT / 'server' / source) for source in sources]
        subprocess.run(command + ['-o', str(cls.binary)], check=True, timeout=120)

    def run_case(self, case):
        result = subprocess.run([str(self.binary), case], capture_output=True, text=True, timeout=15)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_stalled_scalar_tcp(self): self.run_case('tcp-input')
    def test_stalled_scalar_unix(self): self.run_case('unix-input')
    def test_stalled_dynamic_input(self): self.run_case('dynamic-input')
    def test_stalled_vector_output(self): self.run_case('tcp-output')
    def test_stalled_span_output(self): self.run_case('span-output')
    def test_stalled_websocket_output(self): self.run_case('ws-output')
    def test_disconnected_writer(self): self.run_case('disconnect-output')
    def test_concurrent_first_commands(self): self.run_case('concurrent-first')
    def test_truncated_input(self): self.run_case('truncated-input')
    def test_shared_driver_publication(self): self.run_case('manager-publication')


class PreparedResponseTest(unittest.TestCase):
    def test_wire_format_and_borrowed_storage(self):
        with tempfile.TemporaryDirectory() as build:
            binary = Path(build) / 'prepared-response'
            command = [os.environ.get('CXX', 'g++'), '-std=c++20', '-O2', '-Wall',
                       '-Wextra', '-Werror', '-pthread', '-I', str(ROOT),
                       '-I', str(ROOT / 'server/external_libs'),
                       '-I', os.environ.get('EIGEN_INCLUDE_DIR', '/usr/include/eigen3')]
            command += shlex.split(os.environ.get('CXXFLAGS', ''))
            command += [str(ROOT / 'server/tests/prepared_response.cpp'),
                        str(ROOT / 'server/utilities/rate_tracker.cpp'), '-o', str(binary)]
            subprocess.run(command, check=True, timeout=120)
            subprocess.run([str(binary)], check=True, timeout=15)


if __name__ == '__main__':
    unittest.main()
