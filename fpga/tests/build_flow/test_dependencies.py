"""Exercise standard-instrument FPGA rebuild decisions without launching Vivado."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

SDK = Path(__file__).resolve().parents[3]
STANDARD = {
    'red-pitaya/fft': 'fft',
    'red-pitaya/phase-noise-analyzer': 'pna',
    'red-pitaya/adc-dac-bram': 'bram',
    'alpha250/fft': 'fft',
    'alpha250/phase-noise-analyzer': 'pna',
    'alpha250/adc-dac-bram': 'bram',
    'alpha250/adc-dac-dma': 'dma',
    'alpha250-4/fft': 'fft',
    'alpha250-4/phase-noise-analyzer': 'pna',
    'alpha250-4/adc-bram': 'bram',
    'alpha250-4/adc-dma': 'dma',
    'alpha15/signal-analyzer': 'fft',
    'alpha15/adc-dac-bram': 'bram',
    'alpha15/adc-dac-dma': 'dma',
}


class DependenciesTest(unittest.TestCase):
    def test_core_testbench_does_not_invalidate_package(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            core = root / 'test_v1_0'
            core.mkdir()
            for name in ('test.v', 'core_config.tcl', 'README.md'):
                path = core / name
                path.touch()
                os.utime(path, (100, 100))
            output = root / 'output'
            package = output / core.name / 'component.xml'
            package.parent.mkdir(parents=True)
            package.touch()
            # Newer than core.tcl as well as all synthesis inputs.
            timestamp = (SDK / 'fpga/vivado/core.tcl').stat().st_mtime + 10
            os.utime(package, (timestamp, timestamp))
            tb = core / 'test_tb.v'
            tb.touch()
            os.utime(tb, (timestamp + 10, timestamp + 10))
            script = f'set argv [list {{{core}}} test-part {{{output}}}]\nsource {{{SDK}/fpga/vivado/core.tcl}}\n'
            result = subprocess.run(['tclsh'], input=script, capture_output=True, text=True)
            self.assertEqual(result.stderr, '')
            self.assertIn('test_v1_0 up-to-date', result.stdout)
            # Make's settings dependency must bypass the Tcl timestamp cache.
            forced = script.replace(f'{{{output}}}]', f'{{{output}}} 1]')
            result = subprocess.run(['tclsh'], input=forced, capture_output=True, text=True)
            self.assertNotIn('up-to-date', result.stdout)
            self.assertIn('invalid command name "create_project"', result.stderr)
            # A changed packaged guide must still take the packaging path.
            os.utime(core / 'README.md', (timestamp + 10, timestamp + 10))
            result = subprocess.run(['tclsh'], input=script, capture_output=True, text=True)
            self.assertNotIn('up-to-date', result.stdout)
            self.assertIn('invalid command name "create_project"', result.stderr)

    def test_hook_edit_resets_completed_implementation(self):
        # A completed run must be reset before launch_runs; merely rewriting
        # the bitstream would silently ignore a changed timing-repair hook.
        with tempfile.TemporaryDirectory() as tmp:
            script = Path(tmp) / 'run.tcl'
            script.write_text(r"""
set progress 100%
set ::env(ENFORCE_TIMING) 1
proc open_project {args} {}
proc get_runs {args} {return impl_1}
proc get_property {key args} {
    if {$key eq "PROGRESS"} {return $::progress}
    return 0
}
proc reset_run {args} {set ::progress 0%; puts RESET}
proc launch_runs {args} {puts LAUNCH}
proc wait_on_run {args} {}
proc open_run {args} {}
proc current_design {args} {return design}
proc set_property {args} {}
proc write_bitstream {args} {puts WRITE}
proc close_project {args} {}
rename source original_source
proc source {path} {
    if {[file tail $path] eq "timing_check.tcl"} {
        proc koheron_check_routed_timing {args} {puts CHECK}
    } else {
        uplevel 1 [list original_source $path]
    }
}
""" + f'\nsource {{{SDK}/fpga/vivado/bitstream.tcl}}\n')
            for reset in ([], ['0'], ['1']):
                result = subprocess.run(['tclsh', str(script), 'test.xpr', 'test.bit',
                                         'zynq', '8', *reset], capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stderr)
                expected = ['RESET', 'LAUNCH', 'CHECK', 'WRITE'] if reset == ['1'] else ['CHECK', 'WRITE']
                self.assertEqual(result.stdout.splitlines(), expected)

    def test_switching_synthesis_mode_regenerates_project(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            harness = root / 'Makefile'
            harness.write_text(f"""
FPGA_PATH := {SDK}/fpga
TMP_PROJECT_PATH := {root}
NAME := test
%/:
	@mkdir -p $@
include {SDK}/fpga/fpga.mk
$(MEMORY_TCL):
	@mkdir -p $(@D)
	@touch $@
$(TMP_FPGA_PATH)/$(NAME).xpr.stamp:
	@echo $(FPGA_SYNTH_MODE) >> {root}/events
	@touch $@
""")
            for mode in ('global', 'global', 'ip', 'ip', 'global', 'global'):
                result = subprocess.run(['make', '-s', '-f', str(harness),
                                         f'FPGA_SYNTH_MODE={mode}', 'xpr'],
                                        cwd=SDK, capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual((root / 'events').read_text().splitlines(),
                             ['global', 'ip', 'global'])

    def test_tool_mode_and_timing_changes_refresh_only_affected_stages(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            core = root / 'test_v1_0'
            core.mkdir()
            (core / 'test.v').touch()
            (core / 'core_config.tcl').touch()
            (root / 'design.v').touch()
            (root / 'design.tcl').touch()
            tool = root / 'vivado.py'
            tool.write_text('''import os, pathlib, sys
args = sys.argv[1:]
stage = pathlib.Path(args[args.index('-source') + 1]).stem
values = args[args.index('-tclargs') + 1:] if '-tclargs' in args else []
root = pathlib.Path(__file__).parent
with (root / 'events').open('a') as stream:
    stream.write(stage + '\\n')
if stage == 'core':
    assert values[-1] == '1', values
    output = pathlib.Path(values[2], pathlib.Path(values[0]).name, 'component.xml')
elif stage == 'project':
    output = pathlib.Path(os.environ['TMP_FPGA_PATH'], 'test.xpr')
else:
    output = pathlib.Path(values[1])
if stage == 'bitstream' and os.environ['ENFORCE_TIMING'] == '1' and (root / 'fail-timing').exists():
    raise SystemExit(7)
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(stage)
''')
            harness = root / 'Makefile'
            core_selection = root / 'cores.mk'
            core_selection.write_text(f'CORES := {core}\n')
            harness.write_text(f'''SHELL := /bin/bash
.SHELLFLAGS := -eu -o pipefail -c
.DELETE_ON_ERROR:
FPGA_PATH := {SDK}/fpga
TMP_PROJECT_PATH := {root}
PROJECT_PATH := {root}
TCL_FILES := $(wildcard $(PROJECT_PATH)/*.tcl)
NAME := test
PART ?= first
MODE ?= development
VIVADO_VERSION ?= 2025.1
VIVADO_PATH ?= first-path
ENFORCE_TIMING ?= 0
BITSTREAM := {root}/test.bit
include {core_selection}
%/:
\tmkdir -p $@
include {SDK}/fpga/fpga.mk
$(MEMORY_TCL):
\t@mkdir -p $(@D)
\t@touch $@
''')
            def build(*settings, success=True):
                result = subprocess.run(['make', '-j4', '--no-print-directory',
                    '-f', str(harness), f'VIVADO_BATCH={sys.executable} {tool}',
                    *settings, 'fpga'], cwd=SDK,
                    text=True, capture_output=True, timeout=10)
                self.assertEqual(result.returncode == 0, success, result.stdout + result.stderr)
            def events():
                return (root / 'events').read_text().splitlines()
            build()
            self.assertEqual(events(), ['core', 'project', 'hwdef', 'bitstream'])
            stamps = [root / 'fpga/.project-settings', root / 'fpga/.timing-settings',
                      root / 'cores/.vivado-settings', root / 'test.bit']
            times = [p.stat().st_mtime_ns for p in stamps]
            build()
            self.assertEqual(len(events()), 4)
            self.assertEqual(times, [p.stat().st_mtime_ns for p in stamps])
            core_selection.write_text('CORES :=\n')
            before = len(events())
            build()
            self.assertEqual(events()[before:], ['project', 'hwdef', 'bitstream'])
            core_selection.write_text(f'CORES := {core}\n')
            before = len(events())
            build()
            self.assertEqual(events()[before:], ['project', 'hwdef', 'bitstream'])
            for settings in (['MODE=production'], [], ['FPGA_SYNTH_MODE=ip'], []):
                before = len(events())
                build(*settings)
                self.assertEqual(events()[before:], ['project', 'hwdef', 'bitstream'])
            for settings in (['VIVADO_VERSION=2026.1'], [], ['VIVADO_PATH=other-path'], [],
                             ['PART=second'], []):
                before = len(events())
                build(*settings)
                self.assertEqual(events()[before:], ['core', 'project', 'hwdef', 'bitstream'])
            for source, stages in ((core / 'test.v', ['core', 'project', 'hwdef', 'bitstream']),
                                   (root / 'design.v', ['project', 'hwdef', 'bitstream']),
                                   (root / 'design.tcl', ['project', 'hwdef', 'bitstream'])):
                source.unlink()
                before = len(events())
                build()
                self.assertEqual(events()[before:], stages)
                source.touch()
                before = len(events())
                build()
                self.assertEqual(events()[before:], stages)
            (root / 'fail-timing').touch()
            bit_time = (root / 'test.bit').stat().st_mtime_ns
            before = len(events())
            build('ENFORCE_TIMING=1', success=False)
            build('ENFORCE_TIMING=1', success=False)
            self.assertEqual(events()[before:], ['bitstream', 'bitstream'])
            self.assertEqual((root / 'test.bit').stat().st_mtime_ns, bit_time)
            (root / 'fail-timing').unlink()
            build('ENFORCE_TIMING=1')
            before = len(events())
            build('ENFORCE_TIMING=1')
            self.assertEqual(len(events()), before)
            build('ENFORCE_TIMING=invalid', success=False)

    def test_standard_rebuilds(self):
        for instrument, kind in STANDARD.items():
            with self.subTest(instrument=instrument), tempfile.TemporaryDirectory() as tmp:
                harness = Path(tmp) / 'inspect.mk'
                harness.write_text('''\
.PHONY: inspect_outputs
inspect_outputs:
\t@printf '%s\\n' $(CORES_COMPONENT_XML) $(MEMORY_TCL) $(FPGA_PROJECT_STAMP) $(FPGA_TOOL_STAMP) $(FPGA_TIMING_STAMP) $(TMP_FPGA_PATH)/$(NAME).xpr.stamp $(TMP_FPGA_PATH)/$(NAME).xsa $(BITSTREAM) $(PYTHON_REQUIREMENTS_STAMP)
inspect_settings:
\t@printf '%s\\n' $(FPGA_PROJECT_STAMP) $(FPGA_TOOL_STAMP) $(FPGA_TIMING_STAMP) $(CORE_SOURCE_STAMPS)
''')
                cmd = ['make', '--no-print-directory', '-f', 'Makefile', '-f', str(harness),
                       f'CFG=examples/{instrument}/config.mk', f'TMP={tmp}/build',
                       f'PYTHON_REQUIREMENTS_STAMP={tmp}/requirements.stamp']
                env = dict(os.environ, MAKEFLAGS='', MFLAGS='')
                settings = []
                def run(*args):
                    # Dry-run cannot observe cmp preserving a FORCE stamp's mtime.
                    frozen = [arg for path in settings for arg in ('-o', path)] if '-n' in args else []
                    result = subprocess.run([*cmd, *frozen, *args], cwd=SDK, env=env,
                                            capture_output=True, text=True)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    return result.stdout
                settings = run('-s', 'inspect_settings').splitlines()
                run('-s', *settings)
                # Fake only generated outputs; use the real source dependency graph.
                for line in run('-s', 'inspect_outputs').splitlines():
                    path = Path(line)
                    path.parent.mkdir(parents=True, exist_ok=True)
                    path.touch()
                self.assertIn("Nothing to be done", run('-n', 'fpga'))
                board = instrument.split('/')[0]
                board_directory = f'boards/{board}'
                if board == 'red-pitaya':
                    board_directory += '/stemlab-125-14'
                cases = {
                    'make.py': True,
                    'server/__init__.py': True,
                    'fpga/lib/starting_point.tcl': True,
                    'fpga/lib/utilities.tcl': True,
                    'fpga/vivado/block_design.tcl': True,
                    'fpga/lib/dependencies.mk': True,
                    f'{board_directory}/config/board_preset.tcl': True,
                    'fpga/lib/pna_cordic.tcl': kind == 'pna',
                    'fpga/lib/pna_post_route.tcl': 'impl' if kind == 'pna' else False,
                    'fpga/lib/power_spectral_density.tcl': instrument in ('red-pitaya/fft', 'alpha250/fft'),
                    'fpga/lib/pna_single_stream.tcl': instrument in ('red-pitaya/phase-noise-analyzer', 'alpha250/phase-noise-analyzer'),
                    'fpga/lib/bram.tcl': kind in ('bram', 'fft'),
                    'fpga/lib/starting_point_xdma.tcl': False,
                    'fpga/lib/starting_point_zynqmp.tcl': False,
                    'web/plot-references/references.ts': False,
                }
                if board == 'red-pitaya':
                    cases['boards/red-pitaya/stemlab-125-14/config/ports.tcl'] = True
                    cases['fpga/lib/redp_adc_dac.tcl'] = True
                if kind in ('fft', 'pna') and instrument != 'alpha15/signal-analyzer':
                    cases['fpga/cores/latched_mux_v1_0/latched_mux_tb.v'] = False
                    cases['fpga/cores/latched_mux_v1_0/latched_mux.v'] = True
                if kind == 'pna':
                    cases[f'examples/{instrument}/post_route.tcl'] = 'impl'
                if instrument in ('red-pitaya/fft', 'alpha250/fft') or kind == 'pna':
                    cases['fpga/lib/post_route_hold_fix.tcl'] = 'impl'
                if instrument == 'alpha250-4/adc-bram':
                    cases[f'examples/{instrument}/tcl/post_route.tcl'] = 'impl'
                for source, rebuild in cases.items():
                    with self.subTest(source=source):
                        self.assertTrue((SDK / source).is_file())
                        output = run('-n', '-W', source, 'fpga')
                        self.assertEqual('-source ./fpga/vivado/project.tcl' in output,
                                         rebuild is True, output)
                        if rebuild == 'impl':
                            self.assertIn('-source ./fpga/vivado/bitstream.tcl', output)
                            self.assertRegex(output, r'zynq [0-9]+ 1 2>&1')
                            self.assertNotIn('-source ./fpga/vivado/hwdef.tcl', output)
                        elif rebuild is False:
                            self.assertIn("Nothing to be done", output)


if __name__ == '__main__':
    unittest.main()
