"""WebSocket unmasking regressions; the same C++ harness runs on ARM hardware."""
import unittest
from cpp_fixture import compile_cpp, run_cpp, NETWORK_INCLUDES, WEBSOCKET_SOURCES


class WebSocketMaskTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.binary = compile_cpp(cls, 'server/tests/websocket_mask.cpp', *WEBSOCKET_SOURCES,
            flags=('-pthread', '-Wl,--wrap=read'), includes=NETWORK_INCLUDES)

    def test_cases(self):
        for case in (
            'receive-eof', 'boundaries', 'guard-pages', 'receive',
        ):
            with self.subTest(case=case):
                run_cpp(self, self.binary, case, timeout=60)


if __name__ == '__main__':
    unittest.main()
