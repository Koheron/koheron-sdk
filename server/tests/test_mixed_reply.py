"""Mixed reply wire compatibility, borrowed storage and vectored socket writes."""
import unittest
from cpp_fixture import compile_cpp, run_cpp, NETWORK_INCLUDES, WEBSOCKET_SOURCES


class MixedReplyTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.binary = compile_cpp(cls, 'server/tests/mixed_reply.cpp', *WEBSOCKET_SOURCES,
            flags=('-pthread', '-Wl,--wrap=sendmsg', '-Wl,--wrap=__sendmsg64'), includes=NETWORK_INCLUDES)

    def test_cases(self):
        for case in (
            'reference-returns', 'fixed', 'boundaries', 'ownership', 'partial', 'limits',
            'standalone', 'serialization', 'failures',
        ):
            with self.subTest(case=case):
                run_cpp(self, self.binary, case, timeout=30)


if __name__ == '__main__':
    unittest.main()
