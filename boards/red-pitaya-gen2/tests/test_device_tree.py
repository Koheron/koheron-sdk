"""Compile the actual Linux overlays and verify the board memory/identity data."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

SDK = Path(__file__).resolve().parents[3]
BOARDS = {
    'red-pitaya-gen2': '20000000',
    'red-pitaya-pro-gen2': '20000000',
    'red-pitaya-pro-z7020-gen2': '40000000',
}


@unittest.skipUnless(all(shutil.which(tool) for tool in ('gcc', 'dtc', 'fdtget')), 'Requires gcc, dtc and fdtget')
class DeviceTreeTest(unittest.TestCase):
    def test_compiled_board_overlays(self):
        with tempfile.TemporaryDirectory() as directory:
            for board, size in BOARDS.items():
                with self.subTest(board=board):
                    source = SDK / 'boards' / board / 'config/board.dtso'
                    preprocessed = Path(directory) / f'{board}.pp'
                    blob = Path(directory) / f'{board}.dtbo'
                    subprocess.run(['gcc', '-E', '-P', '-x', 'assembler-with-cpp', '-nostdinc', '-undef', '-D__DTS__', '-I', str(SDK), str(source), '-o', str(preprocessed)], check=True)
                    subprocess.run(['dtc', '-@', '-I', 'dts', '-O', 'dtb', '-o', str(blob), str(preprocessed)], check=True)
                    def get(node, property_name, type_name='x'):
                        return subprocess.check_output(['fdtget', '-t', type_name, str(blob), node, property_name], text=True).strip()
                    self.assertEqual(get('/fragment@1/__overlay__/memory@0', 'reg'), f'0 {size}')
                    self.assertEqual(get('/fragment@1/__overlay__/reserved-memory/cma@18000000', 'reg'), '18000000 8000000')
                    self.assertEqual(get('/fragment@2/__overlay__/eeprom@50', 'reg'), '50')
                    self.assertEqual(get('/fragment@0/__overlay__', 'compatible', 's'), f'redpitaya,{board} xlnx,zynq-7000')


if __name__ == '__main__':
    unittest.main()
