"""Check shared hardware staging and vendor BSP concurrency without Xilinx tools."""
from pathlib import Path
import subprocess
import tempfile
import unittest


SDK = Path(__file__).resolve().parents[2]


class FirmwareMakeTest(unittest.TestCase):
    def test_parallel_generators_share_staged_hardware_and_serialize_pmu_bsp(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            fpga = root / 'fpga'
            fpga.mkdir()
            source = fpga / 'test.xsa'
            source.write_text('hardware handoff\n')
            staged = root / 'os/hard/test.xsa'
            fake_hsi = root / 'hsi.py'
            fake_hsi.write_text(f'''\
import os
from pathlib import Path
import sys
script, *args = sys.argv[1:]
if script.endswith('pmufw.tcl'):
    assert os.environ['MAKEFLAGS'] == '-j1'
    destination, hardware = args[2:4]
else:
    destination, hardware = args[3:5]
assert hardware == {str(staged)!r}, hardware
assert Path(hardware).read_text() == 'hardware handoff\\n'
Path(destination, 'Makefile').touch()
''')
            harness = f'''\
SHELL := /bin/bash
.SHELLFLAGS := -eu -o pipefail -c
.DELETE_ON_ERROR:
NAME := test
OS_PATH := {SDK}/os
FPGA_PATH := {SDK}/fpga
TMP_OS_PATH := {root}/os
TMP_FPGA_PATH := {fpga}
HSI := python3 {fake_hsi}
PROC := psu_cortexa53_0
ZYNQ_TYPE := zynqmp
%/:
\tmkdir -p $@
include {SDK}/os/os.mk
'''
            result = subprocess.run(
                ['make', '--no-print-directory', '-j4', '-f', '-',
                 str(root / 'os/fsbl/Makefile'), str(root / 'os/pmu/Makefile')],
                input=harness, text=True, capture_output=True, timeout=10)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual(staged.read_bytes(), source.read_bytes())
            self.assertEqual(result.stdout.count(f'cp {source} {staged}'), 1)


if __name__ == '__main__':
    unittest.main()
