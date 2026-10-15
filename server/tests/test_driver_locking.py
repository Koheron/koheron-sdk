"""Driver locking regressions using production dispatch and real local sockets."""
from pathlib import Path
import tempfile
import unittest
from cpp_fixture import ROOT, compile_cpp, run_cpp, NETWORK_INCLUDES


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
                   'get_fixed_span', 'get_views', 'busy', 'set_scalars', 'fixed_types',
                   'mixed_types', 'empty_arrays', 'large_arrays', 'get_mixed', 'get_owned_vector', 'get_owned_string', 'argument_ownership', 'retained_arguments')
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
                   'tests/driver_locking.cpp')
        cls.binary = compile_cpp(cls, *(ROOT / 'server' / source for source in sources),
            flags=('-pthread', '-DKOHERON_SERVER_BUILD', '-Wl,--wrap=read'),
            includes=(build, ROOT / 'server/tests', *NETWORK_INCLUDES), output=cls.binary)

    def test_cases(self):
        for case in (
            'argument-ownership-tcp', 'argument-ownership-unix', 'argument-ownership-ws',
            'argument-ownership-empty', 'buffered-stream',
            'tcp-read-calls', 'unix-read-calls', 'checked-decode', 'concurrent-first',
            'mixed-disconnect-output', 'disconnect-output', 'empty-arrays', 'fixed-tcp',
            'fixed-ws', 'invalid-driver-tcp', 'invalid-driver-ws', 'large-arrays',
            'maximum-tcp', 'maximum-unix', 'misaligned-tcp', 'misaligned-unix', 'mixed-tcp',
            'mixed-ws', 'oversized-tcp', 'oversized-unix', 'owned-string-tcp',
            'owned-string-unix', 'owned-string-ws', 'owned-vector-output', 'partial-tcp-batch',
            'partial-unix-batch', 'rejected-ws', 'manager-publication', 'dynamic-input',
            'mixed-tcp-output', 'mixed-unix-output', 'mixed-ws-output', 'tcp-input',
            'unix-input', 'span-output', 'tcp-output', 'ws-output', 'truncated-input',
            'truncated-tcp-batch', 'truncated-ws-batch',
        ):
            with self.subTest(case=case):
                run_cpp(self, self.binary, case, timeout=15)


if __name__ == '__main__':
    unittest.main()
