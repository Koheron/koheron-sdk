"""Check formatter execution and output for both compile-time logging modes."""
from pathlib import Path
import tempfile
import unittest
from cpp_fixture import ROOT, compile_cpp, run_cpp


SOURCE = r'''
#include "server/runtime/syslog.hpp"

struct Tracked { int value; };
int format_calls = 0;

template<>
struct std::formatter<Tracked> : std::formatter<int> {
    auto format(const Tracked& value, std::format_context& ctx) const {
        ++format_calls;
        return std::formatter<int>::format(value.value, ctx);
    }
};

int main() {
    logf<DEBUG>("debug {}\n", Tracked{1});
    rt::print_fmt<DEBUG>("direct {}\n", Tracked{2});
    logf<DEBUG>("empty\n");
    const int expected_debug_calls = rt::config::log::verbose ? 2 : 0;
    if (format_calls != expected_debug_calls) return 1;

    logf("info {}\n", Tracked{3});
    logf<WARNING>("warning {}\n", Tracked{4});
    return format_calls == expected_debug_calls + 2 ? 0 : 2;
}
'''


class SyslogTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        build = tempfile.TemporaryDirectory()
        cls.addClassCleanup(build.cleanup)
        build = Path(build.name)
        source = build / 'syslog.cpp'
        source.write_text(SOURCE)
        cls.binaries = {}
        header = (ROOT / 'server/runtime/syslog.hpp').read_text()
        for verbose in (False, True):
            include_dir = build / str(verbose)
            variant = include_dir / 'server/runtime/syslog.hpp'
            variant.parent.mkdir(parents=True)
            # Exercise the enabled mode without changing the production setting.
            variant.write_text(header.replace('constexpr bool verbose = false;',
                                               f'constexpr bool verbose = {str(verbose).lower()};'))
            cls.binaries[verbose] = compile_cpp(cls, source, includes=(include_dir,),
                                              output=include_dir / 'syslog')

    def check_mode(self, verbose, expected_stdout):
        result = run_cpp(self, self.binaries[verbose], timeout=5)
        self.assertEqual(result.stdout, expected_stdout)
        self.assertEqual(result.stderr, 'WARNING: warning 4\n')

    def test_disabled_debug_skips_formatter(self):
        self.check_mode(False, 'info 3\n')

    def test_enabled_debug_formats_and_prints(self):
        self.check_mode(True, 'debug 1\ndirect 2\nempty\ninfo 3\n')


if __name__ == '__main__':
    unittest.main()
