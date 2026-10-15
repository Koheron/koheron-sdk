"""Check shared hardware staging and vendor BSP concurrency without Xilinx tools."""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest


SDK = Path(__file__).resolve().parents[2]


class FirmwareMakeTest(unittest.TestCase):
    def test_standalone_consoles_override_vendor_templates_after_generation(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            bsp = root / 'zynqmp_fsbl_bsp'
            bsp.mkdir()
            (bsp / 'system.mss').touch()
            harness = '''
namespace eval hsi {
    proc open_hw_design {args} { return hw }
    proc create_sw_design {args} {}
    proc add_library {args} {}
    proc generate_app {args} {
        if {[lsearch -exact $args -compile] >= 0} { error "Compile must use the native make rule" }
        set ::selected_console vendor_default
    }
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
set argv ARGUMENTS
if {[info exists env(TEST_CONSOLE)] && $env(TEST_CONSOLE) ne ""} {
    lappend argv $env(TEST_CONSOLE)
}
source SCRIPT
puts "console=$::selected_console"
if {[info exists ::regenerated]} { puts "bsp=$::regenerated" }
'''
            for app, arguments in (
                ('fsbl', f'[list test cpu HARD {root} hardware.xsa zynqmp]'),
                ('pmufw', f'[list test HARD {root} hardware.xsa]'),
            ):
                script = harness.replace('ARGUMENTS', arguments).replace(
                    'SCRIPT', str(SDK / f'fpga/hsi/{app}.tcl'))
                for console in ('', 'psu_uart_1'):
                    with self.subTest(application=app, console=console):
                        result = subprocess.run(['tclsh'], input=script, text=True,
                            capture_output=True, timeout=10,
                            env={**os.environ, 'TEST_CONSOLE': console})
                        self.assertEqual(result.returncode, 0, result.stderr)
                        self.assertEqual(result.stderr, '')
                        self.assertIn('console=' + (console or 'vendor_default'), result.stdout)
                        self.assertEqual('bsp=psu_uart_1' in result.stdout, bool(console))

    def test_pmu_native_compiler_and_flags_replace_cached_application(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            vitis = root / 'vitis'
            compiler = vitis / 'gnu/microblaze/lin/bin/mb-gcc'
            compiler.parent.mkdir(parents=True)
            (vitis / 'settings64.sh').write_text('export PMU_NATIVE_TEST=ready\n')
            compiler.write_text('''\
#!/bin/sh
test "$PMU_NATIVE_TEST" = ready || exit 1
case "$MAKEFLAGS" in *-j1*) ;; *) exit 1 ;; esac
printf '%s\\n' "$*" > executable.elf
''')
            compiler.chmod(0o755)
            fpga = root / 'fpga'
            fpga.mkdir()
            (fpga / 'test.xsa').touch()
            generator = root / 'hsi.py'
            generator.write_text('''\
from pathlib import Path
import sys
firmware = Path(sys.argv[4])
with (firmware.parent / 'generations').open('a') as log:
    log.write('generated\\n')
(firmware / 'Makefile').write_text('CC := mb-gcc\\nall: executable.elf\\n'
    'executable.elf: Makefile\\n\\t$(CC) $(CFLAGS)\\n')
''')
            harness = f'''\
SHELL := /bin/bash
.SHELLFLAGS := -eu -o pipefail -c
NAME := test
TMP_OS_PATH := {root}/os
TMP_FPGA_PATH := {fpga}
OS_PATH := {SDK}/os
FPGA_PATH := {SDK}/fpga
VITIS_PATH := {vitis}
HSI := python3 {generator}
DOCKER := false
%/:
\tmkdir -p $@
include {SDK}/os/os.mk
'''
            output = root / 'os/pmu/executable.elf'
            for flags, generations in (('-DTEST_PLATFORM=1', 1), ('-DTEST_PLATFORM=1', 1),
                                       ('-DTEST_PLATFORM=2', 2), ('', 3), ('', 3)):
                result = subprocess.run(
                    ['make', '--no-print-directory', '-j4', '-f', '-',
                     str(output), f'PMU_CFLAGS={flags}'],
                    input=harness, text=True, capture_output=True, timeout=10)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertEqual(output.read_text(), flags + '\n')
                self.assertEqual((root / 'os/generations').read_text().count('generated'), generations)

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
\t@test -f build/console || printf '%s\\n' '$(ZYNQMP_CONSOLE)' > build/console
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

            def build(version, console='cadence'):
                result = subprocess.run(
                    ['make', '--no-print-directory', '-f', '-', str(output),
                     f'ATF_GCC_VERSION={version}', f'ATF_CONSOLE={console}'], input=harness,
                    text=True, capture_output=True, timeout=10)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertEqual(output.read_text(), f'aarch64-linux-gnu-gcc-{version}\n')
                self.assertEqual((firmware / 'build/console').read_text(), console + '\n')

            build(13)
            build(15)
            events = (firmware / 'events').read_text()
            self.assertEqual(events, 'clean\nbuild\nclean\nbuild\n')
            build(15)
            self.assertEqual((firmware / 'events').read_text(), events)
            build(15, 'cadence1')
            self.assertEqual((firmware / 'events').read_text(), events + 'clean\nbuild\n')
            build(15)
            self.assertEqual((firmware / 'events').read_text(), events + 'clean\nbuild\nclean\nbuild\n')

    def test_parallel_generators_share_staged_hardware_and_track_consoles(self):
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
    destination, hardware = args[2:4]
    with open({str(root / 'pmu-consoles')!r}, 'a') as log:
        log.write(args[4] + '\\n')
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
            for console in ('psu_uart_1', 'psu_uart_1', 'psu_uart_0', ''):
                result = subprocess.run(
                    ['make', '--no-print-directory', '-f', '-',
                     str(root / 'os/pmu/Makefile'), f'PMU_STDIO={console}'],
                    input=harness, text=True, capture_output=True, timeout=10)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual((root / 'pmu-consoles').read_text(),
                             '\npsu_uart_1\npsu_uart_0\n\n')


if __name__ == '__main__':
    unittest.main()
