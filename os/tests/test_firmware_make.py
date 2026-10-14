"""Check shared hardware staging and vendor BSP concurrency without Xilinx tools."""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest


SDK = Path(__file__).resolve().parents[2]


class FirmwareMakeTest(unittest.TestCase):
    def test_fsbl_console_overrides_vendor_template_after_generation(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            bsp = root / 'zynqmp_fsbl_bsp'
            bsp.mkdir()
            (bsp / 'system.mss').touch()
            harness = '''
namespace eval hsi {
    proc open_hw_design {args} {}
    proc create_sw_design {args} {}
    proc add_library {args} {}
    proc generate_app {args} { set ::selected_console vendor_default }
    proc open_sw_design {path} { set ::opened $path }
    proc get_os {} { return os }
    proc generate_bsp {args} { set ::regenerated $::selected_console }
    proc current_hw_design {} { return hw }
    proc close_hw_design {args} {}
}
namespace eval common {
    proc set_property {name value object} {
        if {$name eq "CONFIG.STDOUT"} { set ::selected_console $value }
    }
}
set argv [list test cpu HARD OUTPUT hardware.xsa zynqmp]
if {[info exists env(TEST_CONSOLE)] && $env(TEST_CONSOLE) ne ""} {
    lappend argv $env(TEST_CONSOLE)
}
source SCRIPT
puts "console=$::selected_console"
if {[info exists ::regenerated]} { puts "bsp=$::regenerated" }
'''.replace('OUTPUT', str(root)).replace('SCRIPT', str(SDK / 'fpga/hsi/fsbl.tcl'))
            for console in ('', 'psu_uart_1'):
                result = subprocess.run(['tclsh'], input=harness, text=True,
                    capture_output=True, timeout=10,
                    env={**os.environ, 'TEST_CONSOLE': console})
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(result.stderr, '')
                self.assertIn('console=' + (console or 'vendor_default'), result.stdout)
                self.assertEqual('bsp=psu_uart_1' in result.stdout, bool(console))

    def test_atf_compiler_change_discards_cached_objects(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            firmware = root / 'firmware'
            firmware.mkdir()
            for name in ('checksums', 'download', 'archive'):
                (root / name).touch()
            (firmware / '.unpacked').touch()
            (firmware / 'Makefile').write_text('''\
.PHONY: clean bl31
clean:
\trm -rf build
\t@echo clean >> events
bl31:
\t@mkdir -p build/zynqmp/release/bl31
\t@test -f build/compiler || printf '%s\\n' '$(CC)' > build/compiler
\t@cp build/compiler build/zynqmp/release/bl31/bl31.elf
\t@echo build >> events
''')
            output = root / 'os/bl31.elf'
            harness = f'''\
SHELL := /bin/bash
.SHELLFLAGS := -eu -o pipefail -c
OS_PATH := {SDK}/os
TMP_OS_PATH := {root}/os
ATRUST_PATH := {firmware}
ATRUST_TAR := {root}/archive
SOURCE_CHECKSUMS := {root}/checksums
DOWNLOAD_VERIFIED := {root}/download
GCC_ARCH := aarch64-linux-gnu
%/:
\tmkdir -p $@
include {SDK}/os/os.mk
'''
            output.parent.mkdir()

            def build(version):
                result = subprocess.run(
                    ['make', '--no-print-directory', '-f', '-', str(output),
                     f'ATF_GCC_VERSION={version}'], input=harness,
                    text=True, capture_output=True, timeout=10)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertEqual(output.read_text(), f'aarch64-linux-gnu-gcc-{version}\n')

            build(13)
            build(15)
            events = (firmware / 'events').read_text()
            self.assertEqual(events, 'clean\nbuild\nclean\nbuild\n')
            build(15)
            self.assertEqual((firmware / 'events').read_text(), events)

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
    with open({str(root / 'fsbl-consoles')!r}, 'a') as log:
        log.write((args[6] if len(args) > 6 else 'default') + '\\n')
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
            self.assertEqual((root / 'fsbl-consoles').read_text(), 'default\n')
            for console in ('psu_uart_1', 'psu_uart_1', 'psu_uart_0'):
                result = subprocess.run(
                    ['make', '--no-print-directory', '-f', '-',
                     str(root / 'os/fsbl/Makefile'), f'FSBL_STDIO={console}'],
                    input=harness, text=True, capture_output=True, timeout=10)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual((root / 'fsbl-consoles').read_text(),
                             'default\npsu_uart_1\npsu_uart_0\n')


if __name__ == '__main__':
    unittest.main()
