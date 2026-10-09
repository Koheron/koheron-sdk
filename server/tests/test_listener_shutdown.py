"""Listener shutdown and admission limits with real TCP/WebSocket/UNIX sockets."""
import os
from pathlib import Path
import shlex
import socket
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


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
        command = [os.environ.get('CXX', 'g++'), '-std=c++20', '-O2', '-Wall',
                   '-Wextra', '-Werror', '-pthread', '-Wl,--wrap=read',
                   '-I', str(build), '-I', str(ROOT),
                   '-I', str(ROOT / 'server/external_libs'),
                   '-I', os.environ.get('EIGEN_INCLUDE_DIR', '/usr/include/eigen3')]
        command += shlex.split(os.environ.get('CXXFLAGS', ''))
        sources = ('tests/listener_shutdown.cpp', 'network/listener_manager.cpp',
                   'network/listening_channel.cpp', 'network/session_manager.cpp',
                   'network/session.cpp', 'network/sockets.cpp', 'network/websocket.cpp',
                   'network/sha1.cpp', 'network/base64.cpp', 'runtime/runtime_executor.cpp',
                   'utilities/rate_tracker.cpp')
        command += [str(ROOT / 'server' / source) for source in sources]
        subprocess.run(command + ['-o', str(cls.binary)], check=True, timeout=120)

    def run_case(self, case):
        result = subprocess.run([str(self.binary), case], capture_output=True,
                                text=True, timeout=15)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_shutdown_waits_for_blocked_workers(self): self.run_case('shutdown')
    def test_connection_limit_during_burst(self): self.run_case('limit')


if __name__ == '__main__':
    unittest.main()
