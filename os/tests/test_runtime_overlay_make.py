"""Compile the runtime overlay with the real make recipes and resolve fixups."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

SDK = Path(__file__).resolve().parents[2]


@unittest.skipUnless(all(shutil.which(t) for t in ('dtc', 'fdtget', 'fdtoverlay')),
                     'requires device-tree-compiler')
class RuntimeOverlayMakeTest(unittest.TestCase):
    def test_device_overlay_avoids_permanent_properties_and_keeps_fixups(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            overlay = root / 'os/pl-overlay'
            overlay.mkdir(parents=True)
            hard = root / 'os/hard/test.xsa'
            hard.parent.mkdir(); hard.touch(); os.utime(hard, (1, 1))
            source = root / 'fpga/test.xsa'
            source.parent.mkdir(); source.touch(); os.utime(source, (1, 1))
            unpacked = root / 'os/device-tree-xlnx-xilinx_v2025.1/.unpacked'
            unpacked.parent.mkdir(); unpacked.touch(); os.utime(unpacked, (1, 1))
            archive = root / 'device-tree-xlnx-xilinx_v2025.1.tar.gz'
            archive.touch(); os.utime(archive, (1, 1))
            memory = root / 'memory.yml'; memory.touch()
            (overlay / 'memory.dtsi').write_text('')
            (overlay / 'pl.dtsi').write_text('''\
&fpga_full {
    firmware-name = ".bin";
    sample_clk: sample-clock {
        compatible = "fixed-clock";
        #clock-cells = <0>;
        clock-frequency = <250000000>;
    };
    peripheral: test-device { clocks = <&sample_clk>; };
};
''')
            harness = f'''\
SHELL := /bin/bash
.SHELLFLAGS := -eu -o pipefail -c
.DELETE_ON_ERROR:
NAME := test
OS_PATH := {SDK}/os
FPGA_PATH := {SDK}/fpga
TMP_OS_PATH := {root}/os
TMP := {root}
TMP_FPGA_PATH := {root}/fpga
MEMORY_YML := {memory}
VIVADO_VERSION := 2025.1
DTC_BIN := {shutil.which('dtc')}
%/:
\tmkdir -p $@
.PHONY: FORCE
FORCE:
include {SDK}/os/os.mk
'''
            target = root / 'os/pl.dtbo'
            result = subprocess.run(['make', '--no-print-directory', '-f', '-', str(target)],
                                    input=harness, text=True, capture_output=True, timeout=10)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            nodes = subprocess.check_output(['fdtget', '-l', str(target), '/'], text=True)
            self.assertNotIn('__symbols__', nodes)
            self.assertIn('__fixups__', nodes)
            self.assertIn('__local_fixups__', nodes)
            properties = subprocess.check_output(
                ['fdtget', '-p', str(target), '/fragment@0/__overlay__'], text=True)
            self.assertNotIn('firmware-name', properties)
            base = root / 'base.dts'
            base.write_text('/dts-v1/; / { fpga_full: fpga-region { compatible = "fpga-region"; }; };')
            subprocess.run(['dtc', '-@', '-I', 'dts', '-O', 'dtb', '-o', str(root / 'base.dtb'), str(base)], check=True)
            merged = root / 'merged.dtb'
            subprocess.run(['fdtoverlay', '-i', str(root / 'base.dtb'), '-o', str(merged), str(target)], check=True)
            clock = subprocess.check_output(['fdtget', '-t', 'x', str(merged), '/fpga-region/sample-clock', 'phandle'], text=True)
            reference = subprocess.check_output(['fdtget', '-t', 'x', str(merged), '/fpga-region/test-device', 'clocks'], text=True)
            self.assertEqual(reference, clock)
            # Re-running must not rewrite an unchanged normalized source.
            stamp = (overlay / 'pl-koheron.dtsi').stat().st_mtime_ns
            result = subprocess.run(['make', '--no-print-directory', '-f', '-', str(target)],
                                    input=harness, text=True, capture_output=True, timeout=10)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual((overlay / 'pl-koheron.dtsi').stat().st_mtime_ns, stamp)


if __name__ == '__main__':
    unittest.main()
