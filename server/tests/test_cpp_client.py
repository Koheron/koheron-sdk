"""Compile the C++ client and exercise it against local HTTP/TCP servers."""
import http.server
import json
import os
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


class CppClientTest(unittest.TestCase):
    def test_client(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp)
            (tmp / "operations.hpp").write_text(OPERATIONS)
            binary = tmp / "cpp-client-test"
            subprocess.run([
                os.environ.get("CXX", "g++"), "-std=c++20", "-Wall", "-Wuseless-cast",
                "-Werror", "-pthread", "-I", str(tmp), "-I", str(ROOT),
                str(ROOT / "server/tests/cpp_client.cpp"), "-o", str(binary),
            ], check=True)
            with socketserver.ThreadingTCPServer(("127.0.0.1", 0), CommandHandler) as tcp, \
                    http.server.ThreadingHTTPServer(("127.0.0.1", 0), StatusHandler) as http_srv:
                threads = [threading.Thread(target=s.serve_forever) for s in (tcp, http_srv)]
                for thread in threads:
                    thread.start()
                try:
                    subprocess.run([str(binary), str(tcp.server_address[1]),
                                    str(http_srv.server_address[1])], check=True, timeout=15)
                finally:
                    tcp.shutdown()
                    http_srv.shutdown()
                    for thread in threads:
                        thread.join()


if __name__ == "__main__":
    unittest.main()
