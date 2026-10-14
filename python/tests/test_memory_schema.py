"""Validation and all memory-map outputs must agree on numeric sizes."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

import yaml
from koheron.memory_schema import parse_int, parse_range, size_bytes
from koheron import validate_config

SDK = Path(__file__).resolve().parents[2]


class MemorySchemaTest(unittest.TestCase):
    def test_supported_ranges(self):
        for value, expected in ((4096, 4096), ('4096', 4096), ('4k', 4096),
                                (' 4_K ', 4096), ('0x10k', 16384),
                                ('2m', 2 * 1024**2), ('1g', 1024**3),
                                ('0x1_000', 4096), ('0b1000', 8), ('08', 8)):
            with self.subTest(value=value):
                self.assertEqual(parse_range(value), expected)
                self.assertEqual(validate_config.parse_range(value), expected)
                self.assertEqual(size_bytes(value), expected)

    def test_invalid_ranges(self):
        for value in (True, False, None, 1.5, '', 'four', '4KiB', [], {}, '4 K'):
            with self.subTest(value=value):
                self.assertIsNone(parse_range(value))
                with self.assertRaises(ValueError): size_bytes(value)
        for value in (0, -4096, '0K', '-1m'):
            with self.subTest(value=value), self.assertRaises(ValueError): size_bytes(value)

    def test_offsets_and_booleans(self):
        self.assertEqual(parse_int('0x4000_0000'), 0x40000000)
        self.assertEqual(parse_int('4096'), 4096)
        self.assertIsNone(parse_int(True))
        self.assertIsNone(parse_int('4K'))

    def test_generated_sizes_and_block_offsets_agree(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            memory = root / 'memory.yml'
            for value in ('4k', '0x1_0k', 4096, '0x1_000'):
                with self.subTest(value=value):
                    size = size_bytes(value)
                    memory.write_text(yaml.safe_dump({'parameters': {}, 'memory': [
                        {'name': 'ram', 'offset': '0x4000_0000', 'range': value,
                         'compatible': 'koheron,mem-simple', 'registers': []},
                        {'name': 'banks[2]', 'offset': '0x5000_0000', 'range': value,
                         'registers': []}]}))
                    outputs = {}
                    for kind, suffix in (('memory_hpp', 'hpp'), ('memory_tcl', 'tcl'), ('memory_dtsi', 'dtsi')):
                        output = root / f'memory.{suffix}'
                        result = subprocess.run([sys.executable, str(SDK / 'make.py'), '--' + kind,
                                                 str(output), str(memory)], cwd=SDK, text=True,
                                                capture_output=True, timeout=15,
                                                env={**os.environ, 'SDK_PATH': str(SDK), 'ARCH': 'armhf'})
                        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                        outputs[suffix] = output.read_text()
                    self.assertIn(f'ram_range = {size}U;', outputs['hpp'])
                    self.assertIn(f'* {size}]]', outputs['tcl'])
                    self.assertNotIn('1024U', outputs['tcl'])
                    self.assertIn(f'0x{size:08x}', outputs['dtsi'])
                    # Execute the generated Tcl, exercising real integer arithmetic.
                    tcl = subprocess.run(['tclsh'], input=outputs['tcl'] +
                                         '\nputs "$config::memory_banks0_offset $config::memory_banks1_offset"\n',
                                         text=True, capture_output=True, timeout=5)
                    self.assertEqual(tcl.returncode, 0, tcl.stderr)
                    self.assertEqual(tcl.stdout.strip(), f'0x50000000 0x{0x50000000 + size:x}')


if __name__ == '__main__':
    unittest.main()
