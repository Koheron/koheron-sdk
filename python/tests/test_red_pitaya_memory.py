"""Keep PNA's FPGA, server and runtime overlay inside each board's DMA tail."""
import copy
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

import yaml

SDK = Path(__file__).resolve().parents[2]
PNA = SDK / 'examples/red-pitaya/phase-noise-analyzer'


class RedPitayaMemoryTest(unittest.TestCase):
    def test_generated_dma_addresses_agree_and_fit_reserved_ddr(self):
        models = (
            ('stemlab-125-14', 0x20000000, 0x08000000),
            ('stemlab-125-14-gen2', 0x20000000, 0x08000000),
            ('stemlab-125-14-pro-gen2', 0x20000000, 0x08000000),
            ('stemlab-125-14-pro-z7020-gen2', 0x40000000, 0x20000000),
        )
        environment = {**os.environ, 'MAKEFLAGS': '', 'MFLAGS': '', 'SDK_PATH': str(SDK), 'ARCH': 'armhf'}
        with tempfile.TemporaryDirectory() as directory:
            temporary = Path(directory)
            for selector, ddr_size, reserved_size in models:
                with self.subTest(selector=selector):
                    info = subprocess.run(
                        ['make', '--no-print-directory', 'info', f'CFG={PNA / (selector + ".mk")}', 'FORMAT=json'],
                        cwd=SDK, env=environment, capture_output=True, text=True, check=True, timeout=15)
                    settings = json.loads(info.stdout)['settings']
                    source = Path(settings['MEMORY_YML'])
                    memory = yaml.safe_load(source.read_text())
                    ram = next(region for region in memory['memory'] if region['name'] == 'ram')
                    address = ram['offset']
                    self.assertEqual(ram['range'], '32M')
                    self.assertGreaterEqual(address, ddr_size - reserved_size)
                    self.assertEqual(address + 32 * 1024 * 1024, ddr_size)
                    outputs = {}
                    for option, extension in (('memory_hpp', 'hpp'), ('memory_tcl', 'tcl'), ('memory_dtsi', 'dtsi')):
                        output = temporary / f'{selector}.{extension}'
                        subprocess.run([sys.executable, str(SDK / 'make.py'), '--' + option, str(output), str(source)],
                                       cwd=SDK, env=environment, capture_output=True, text=True, check=True, timeout=15)
                        outputs[extension] = output.read_text()
                    self.assertIn(f'ram_addr = 0x{address:X};', outputs['hpp'])
                    self.assertIn(f'ram_dev = "/dev/mem_wc0x{address:X}";', outputs['hpp'])
                    self.assertIn(f'memory_ram_offset 0x{address:X}', outputs['tcl'])
                    self.assertIn(f'reg = <0x{address:08x} 0x02000000>;', outputs['dtsi'])

    def test_one_gib_map_preserves_registers_and_processing_parameters(self):
        original = yaml.safe_load((PNA / 'memory.yml').read_text())
        larger = yaml.safe_load((PNA / 'memory-1g.yml').read_text())
        normalized = copy.deepcopy(larger)
        ram = next(region for region in normalized['memory'] if region['name'] == 'ram')
        ram['offset'] = next(region['offset'] for region in original['memory'] if region['name'] == 'ram')
        self.assertEqual(normalized, original)


if __name__ == '__main__':
    unittest.main()
