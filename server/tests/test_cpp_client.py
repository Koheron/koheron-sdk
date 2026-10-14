"""Compile the C++ client and exercise it against local HTTP/TCP servers."""
import http.server
import json
import os
import shlex
from pathlib import Path
import socketserver
import struct
import subprocess
import tempfile
import threading
import unittest


ROOT = Path(__file__).resolve().parents[2]
CONTEXT = json.dumps([
    {"class": "KServer", "id": 1, "functions": [
        {"name": "get_version", "id": 0, "args": [], "ret_type": "const char *"},
        {"name": "get_cmds", "id": 1, "args": [], "ret_type": "std::string"},
    ]},
    {"class": "TestDriver", "id": 2, "functions": [
        {"name": "set_value", "id": 0, "ret_type": "void",
         "args": [{"name": "value", "type": "uint32_t"}]},
        {"name": "get_value", "id": 1, "ret_type": "uint32_t", "args": []},
    ]},
], separators=(",", ":")).encode()

OPERATIONS = """
#include <cstdint>
#include <tuple>
namespace op::TestDriver { constexpr uint32_t get_value = (2U << 16) | 1; }
template<uint32_t id> struct arg_types;
template<uint32_t id> using arg_types_t = typename arg_types<id>::type;
template<uint32_t id> struct ret_type;
template<uint32_t id> using ret_type_t = typename ret_type<id>::type;
template<> struct arg_types<op::TestDriver::get_value> { using type = std::tuple<>; };
template<> struct ret_type<op::TestDriver::get_value> { using type = uint32_t; };
"""


class CommandHandler(socketserver.StreamRequestHandler):
    def handle(self):
        self.request.settimeout(5)
        value = 0
        while header := self.rfile.read(8):
            reserved, class_id, func_id = struct.unpack("!IHH", header)
            if reserved != 0:
                raise ValueError("Unexpected reserved field")
            if class_id == 1:
                body = b"1.0" if func_id == 0 else CONTEXT
                self.request.sendall(header + struct.pack("!I", len(body)) + body)
            elif (class_id, func_id) == (2, 0):
                value, = struct.unpack("!I", self.rfile.read(4))
            elif (class_id, func_id) == (2, 1):
                self.request.sendall(header + struct.pack("!I", value))
            else:
                raise ValueError("Unexpected operation")


class StatusHandler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        body = b'{"instruments":["test"],"live_instrument":"test"}'
        self.send_response(200 if self.path == "/api/instruments" else 404)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *args):
        pass


class MissingContextHandler(socketserver.BaseRequestHandler):
    def handle(self):
        self.request.recv(8)


class CppClientTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        directory = tempfile.TemporaryDirectory()
        cls.addClassCleanup(directory.cleanup)
        tmp = Path(directory.name)
        (tmp / "operations.hpp").write_text(OPERATIONS)
        cls.binaries = {}
        for name in ('cpp_client', 'cpp_client_lifecycle'):
            binary = tmp / name
            subprocess.run([
                os.environ.get("CXX", "g++"), "-std=c++20", "-Wall", "-Wuseless-cast",
                "-Werror", "-pthread", "-I", str(tmp), "-I", str(ROOT),
                *shlex.split(os.environ.get('CXXFLAGS', '')),
                str(ROOT / f"server/tests/{name}.cpp"), "-o", str(binary),
            ], check=True, timeout=90)
            cls.binaries[name] = binary

    def run_client(self, case=None, handler=CommandHandler):
        with socketserver.ThreadingTCPServer(("127.0.0.1", 0), handler) as tcp, \
                http.server.ThreadingHTTPServer(("127.0.0.1", 0), StatusHandler) as http_srv:
            threads = [threading.Thread(target=s.serve_forever) for s in (tcp, http_srv)]
            for thread in threads:
                thread.start()
            try:
                name = 'cpp_client_lifecycle' if case else 'cpp_client'
                argument = case or str(http_srv.server_address[1])
                result = subprocess.run([str(self.binaries[name]), str(tcp.server_address[1]), argument],
                                        capture_output=True, text=True, timeout=15)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            finally:
                tcp.shutdown()
                http_srv.shutdown()
                for thread in threads:
                    thread.join()

    def test_client(self): self.run_client()
    def test_repeated_close_and_descriptor_reuse(self): self.run_client('close')
    def test_descriptor_zero_is_owned(self): self.run_client('zero')
    def test_move_preserves_connection_and_metadata(self): self.run_client('move')
    def test_move_assignment_releases_old_connection(self): self.run_client('assign')
    def test_reconnect_releases_old_connection(self): self.run_client('reconnect')
    def test_failed_connection_preserves_error_and_can_retry(self): self.run_client('refused')
    def test_failed_metadata_releases_connection(self): self.run_client('context', MissingContextHandler)


if __name__ == "__main__":
    unittest.main()
