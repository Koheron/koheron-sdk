"""Listener shutdown and admission limits with real TCP/WebSocket/UNIX sockets."""
from pathlib import Path
import socket
import tempfile
import unittest
from cpp_fixture import ROOT, compile_cpp, run_cpp, NETWORK_INCLUDES


class ListenerShutdownTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        directory = tempfile.TemporaryDirectory()
        cls.addClassCleanup(directory.cleanup)
        build = Path(directory.name)
        def port():
            with socket.socket() as sock:
                sock.bind(('127.0.0.1', 0))
                return sock.getsockname()[1]
        tcp, ws = port(), port()
        while tcp == ws:
            ws = port()
        config = ROOT / 'server/network/configs/config.hpp'
        override = build / 'server/network/configs/config.hpp'
        override.parent.mkdir(parents=True)
        override.write_text(config.read_text()
                            .replace('tcp_port = 36000', f'tcp_port = {tcp}')
                            .replace('websocket_port = 8080', f'websocket_port = {ws}')
                            .replace('tcp_worker_connections = 100', 'tcp_worker_connections = 4')
                            .replace('/var/run/koheron-server.sock', str(build / 'server.sock')))
        (build / 'drivers_list.hpp').write_text('#pragma once\n#include <tuple>\nusing driver_list = std::tuple<>;\n')
        cls.binary = build / 'listener-shutdown'
        sources = ('tests/listener_shutdown.cpp', 'network/listener_manager.cpp',
                   'network/listening_channel.cpp', 'network/session_manager.cpp',
                   'network/session.cpp', 'network/sockets.cpp', 'network/websocket.cpp',
                   'network/sha1.cpp', 'network/base64.cpp', 'runtime/runtime_executor.cpp')
        cls.binary = compile_cpp(cls, *(ROOT / 'server' / source for source in sources),
            flags=('-pthread', '-Wl,--wrap=read'), includes=(build, *NETWORK_INCLUDES),
            output=cls.binary)

    def test_cases(self):
        for case in (
            'limit', 'shutdown',
        ):
            with self.subTest(case=case):
                run_cpp(self, self.binary, case, timeout=15)


if __name__ == '__main__':
    unittest.main()
