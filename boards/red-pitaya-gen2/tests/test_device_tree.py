"""Verify Linux DMA reservations and U-Boot allocation limits from the board DTS."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

SDK = Path(__file__).resolve().parents[3]
BOARDS = {
    'red-pitaya-gen2': (0x20000000, 0x08000000),
    'red-pitaya-pro-gen2': (0x20000000, 0x08000000),
    'red-pitaya-pro-z7020-gen2': (0x40000000, 0x20000000),
}


@unittest.skipUnless(all(shutil.which(tool) for tool in ('gcc', 'dtc', 'fdtget')), 'Requires gcc, dtc and fdtget')
class DeviceTreeTest(unittest.TestCase):
    def test_boot_allocations_stay_below_dma(self):
        # Only peripheral labels and bus cells are needed to compile the actual
        # board include chain without downloading the vendor U-Boot sources.
        soc = '''/ {
            #address-cells = <1>;
            #size-cells = <1>;
            uart0: serial@5 { reg = <5 1>; };
            sdhci0: mmc@1 { reg = <1 1>; };
            gem0: ethernet@2 {
                reg = <2 1>; #address-cells = <1>; #size-cells = <0>;
            };
            i2c0: i2c@3 {
                reg = <3 1>; #address-cells = <1>; #size-cells = <0>;
            };
            usb0: usb@4 { reg = <4 1>; };
        };'''
        patches = SDK / 'boards/red-pitaya/patches/u-boot/arch/arm/dts'
        with tempfile.TemporaryDirectory() as directory:
            temporary = Path(directory)
            (temporary / 'zynq-7000.dtsi').write_text(soc)
            for board, (size, dma_size) in BOARDS.items():
                with self.subTest(board=board):
                    source = patches / f'zynq-{board}.dts'
                    preprocessed = temporary / f'{board}.pp'
                    blob = temporary / f'{board}.dtb'
                    subprocess.run(['gcc', '-E', '-P', '-x', 'assembler-with-cpp', '-nostdinc', '-undef', '-I', str(temporary), str(source), '-o', str(preprocessed)], check=True)
                    subprocess.run(['dtc', '-I', 'dts', '-O', 'dtb', '-o', str(blob), str(preprocessed)], check=True)
                    memory = subprocess.check_output(['fdtget', '-t', 'x', str(blob), '/memory@0', 'reg'], text=True).strip()
                    self.assertEqual(memory, f'0 {size - dma_size:x}')

    def test_compiled_board_overlays(self):
        with tempfile.TemporaryDirectory() as directory:
            for board, (size, dma_size) in BOARDS.items():
                with self.subTest(board=board):
                    source = SDK / 'boards' / board / 'config/board.dtso'
                    preprocessed = Path(directory) / f'{board}.pp'
                    blob = Path(directory) / f'{board}.dtbo'
                    subprocess.run(['gcc', '-E', '-P', '-x', 'assembler-with-cpp', '-nostdinc', '-undef', '-D__DTS__', '-I', str(SDK), str(source), '-o', str(preprocessed)], check=True)
                    subprocess.run(['dtc', '-@', '-I', 'dts', '-O', 'dtb', '-o', str(blob), str(preprocessed)], check=True)
                    def get(node, property_name, type_name='x'):
                        return subprocess.check_output(['fdtget', '-t', type_name, str(blob), node, property_name], text=True).strip()
                    self.assertEqual(get('/fragment@1/__overlay__/memory@0', 'reg'), f'0 {size:x}')
                    dma_base = size - dma_size
                    dma_node = f'/fragment@1/__overlay__/reserved-memory/dma-buffer@{dma_base:x}'
                    self.assertEqual(get(dma_node, 'reg'), f'{dma_base:x} {dma_size:x}')
                    self.assertEqual(get(dma_node, 'no-map'), '')
                    properties = subprocess.check_output(['fdtget', '-p', str(blob), dma_node], text=True).split()
                    self.assertNotIn('reusable', properties)
                    self.assertNotIn('linux,cma', properties)
                    self.assertNotIn('compatible', properties)
                    self.assertEqual(get('/fragment@2/__overlay__/eeprom@50', 'reg'), '50')
                    self.assertEqual(get('/fragment@0/__overlay__', 'compatible', 's'), f'redpitaya,{board} xlnx,zynq-7000')


if __name__ == '__main__':
    unittest.main()
