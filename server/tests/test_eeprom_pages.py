"""EEPROM page writes, bounds and failed serial reads for both ALPHA drivers."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
BOARDS = ('alpha250', 'alpha15')

class EepromPagesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.directory.cleanup)
        cls.root = Path(cls.directory.name)
        cls.compiler = [os.environ.get('CXX', 'g++'), '-std=c++23', '-O2', '-Wall',
                        '-Wextra', '-Werror', '-fno-exceptions', '-pthread', '-I', str(ROOT)]
        cls.compiler += shlex.split(os.environ.get('CXXFLAGS', ''))
        for board in BOARDS:
            command = cls.compiler + [f'-DEEPROM_HEADER="boards/{board}/drivers/eeprom.hpp"',
                str(ROOT / 'server/tests/eeprom_pages.cpp'), '-o', str(cls.root / board)]
            subprocess.run(command, check=True, timeout=90)

    def run_case(self, case):
        for board in BOARDS:
            with self.subTest(board=board):
                result = subprocess.run([str(self.root / board), case], capture_output=True,
                                        text=True, timeout=10)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_every_page_offset_preserves_neighbors(self): self.run_case('offsets')
    def test_aligned_packets_and_transaction_count(self): self.run_case('aligned')
    def test_typed_data_across_boundary(self): self.run_case('typed')
    def test_last_byte_and_page(self): self.run_case('end')
    def test_empty_operations_do_not_touch_device(self): self.run_case('empty')
    def test_serial_errors_do_not_expose_partial_data(self): self.run_case('serial')
    def test_busy_retry_between_pages_and_readback(self): self.run_case('busy')

    def test_invalid_ranges_and_nontrivial_objects_do_not_compile(self):
        cases = [('-1', 'uint32_t', '1', 'out of EEPROM'),
                 ('8192', 'uint8_t', '1', 'out of EEPROM'),
                 ('0', 'uint8_t', '8193', 'out of EEPROM'),
                 ('0', 'std::string', '1', 'raw object bytes')]
        for board in BOARDS:
            for offset, element, count, diagnostic in cases:
                with self.subTest(board=board, offset=offset, element=element, count=count):
                    source = self.root / 'invalid.cpp'
                    source.write_text(f'''#include "boards/{board}/drivers/eeprom.hpp"
void invalid(Eeprom& e) {{
    std::array<{element}, {count}> data{{}};
    e.write<{offset}>(data);
    e.read<{offset}>(data);
}}
''')
                    result = subprocess.run(self.compiler + ['-fsyntax-only', str(source)],
                                            capture_output=True, text=True, timeout=30)
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn(diagnostic, result.stderr)

if __name__ == '__main__':
    unittest.main()
