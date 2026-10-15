"""WebSocket send regressions using production framing and real local sockets."""
import unittest
from cpp_fixture import compile_cpp, run_cpp, NETWORK_INCLUDES, WEBSOCKET_SOURCES


class WebSocketSendTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.binary = compile_cpp(cls, 'server/tests/websocket_send.cpp', *WEBSOCKET_SOURCES,
            flags=('-pthread', '-Wl,--wrap=sendmsg'), includes=NETWORK_INCLUDES)

    def test_cases(self):
        for case in (
            'fragmented', 'boundaries', 'partial', 'failures', 'single-limit',
        ):
            with self.subTest(case=case):
                run_cpp(self, self.binary, case, timeout=15)


if __name__ == '__main__':
    unittest.main()
