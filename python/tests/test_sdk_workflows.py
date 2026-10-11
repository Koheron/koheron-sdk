"""Host workflow contracts: resolved settings, read-only diagnostics and checks."""
import contextlib
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

from koheron import check, doctor
from koheron.diagnostics import Reporter

SDK = Path(__file__).resolve().parents[2]
CFG = 'examples/red-pitaya/fft/config.mk'


class MakeWorkflowTest(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        self.cache = self.root / 'cache'

    def make(self, *args, success=True):
        result = subprocess.run(['make', '--no-print-directory', *args], cwd=SDK,
            capture_output=True, text=True, timeout=15,
            env={**os.environ, 'MAKEFLAGS': '', 'MFLAGS': ''})
        self.assertEqual(result.returncode == 0, success, result.stdout + result.stderr)
        return result

    def test_info_resolves_overrides_without_creating_artifacts(self):
        result = self.make('info', f'CFG={CFG}', f'TMP={self.cache}', 'FORMAT=json',
                           'VERBOSE=1', 'MODE=production', 'N_CPUS=3', 'GCC_VERSION=15',
                           'UBOOT_VERSION=2025.1', 'MEMORY_YML=/custom/map.yml')
        values = json.loads(result.stdout)['settings']
        self.assertEqual(values['BOARD'], 'red-pitaya')
        self.assertEqual(values['UBUNTU_ARCH'], 'armhf')
        self.assertEqual(values['MODE'], 'production')
        self.assertEqual(values['N_CPUS'], '3')
        self.assertEqual(values['UBOOT_VERSION'], '2025.1')
        self.assertEqual(values['MEMORY_YML'], '/custom/map.yml')
        self.assertEqual(values['KERNEL_CC'], 'arm-linux-gnueabihf-gcc-15')
        self.assertEqual(values['INSTRUMENT_TEST'], str(SDK / 'examples/red-pitaya/fft/python/test.py'))
        self.assertTrue(values['INSTRUMENT_ZIP'].startswith(str(self.cache)))
        self.assertFalse(self.cache.exists())
        self.assertNotIn('\033', result.stdout)

    def test_info_resolves_arm64_board_and_requires_cfg(self):
        result = self.make('info', 'CFG=examples/kria-kr260/test/config.mk', 'FORMAT=json', f'TMP={self.cache}')
        values = json.loads(result.stdout)['settings']
        self.assertEqual(values['UBUNTU_ARCH'], 'arm64')
        self.assertEqual(values['GCC_ARCH'], 'aarch64-linux-gnu')
        self.assertFalse(self.cache.exists())
        self.assertIn('CFG is not defined', self.make('info', success=False).stderr)

    def test_doctor_uses_resolved_memory_selection(self):
        memory = self.root / 'selected.yml'
        memory.write_text('memory: []\n')
        result = self.make('doctor', 'TARGET=check', f'CFG={CFG}', 'FORMAT=json',
                           f'TMP={self.cache}', f'MEMORY_YML={memory}', f'HOST_PYTHON={sys.executable}',
                           'VIVADO_PATH=/absent', 'VITIS_PATH=/absent')
        report = json.loads(result.stdout)
        self.assertTrue(report['ok'])
        self.assertEqual(report['settings']['MEMORY_YML'], str(memory))
        self.assertFalse(self.cache.exists())

    def test_missing_selected_memory_is_an_actionable_json_failure(self):
        result = self.make('doctor', 'TARGET=check', f'CFG={CFG}', 'FORMAT=json',
                           f'MEMORY_YML={self.root}/missing.yml', f'HOST_PYTHON={sys.executable}', success=False)
        report = json.loads(result.stdout)
        self.assertFalse(report['ok'])
        self.assertTrue(any('Selected memory.yml missing' in entry['message'] for entry in report['checks']))


class DoctorTest(unittest.TestCase):
    def run_doctor(self, target):
        stream = io.StringIO()
        with contextlib.redirect_stdout(stream), patch.dict(os.environ, {'CFG': ''}):
            result = doctor.main(['--sdk-path', str(SDK), '--target', target, '--format', 'json'])
        return result, json.loads(stream.getvalue())

    def test_web_and_server_skip_xilinx_checks(self):
        with patch.object(doctor, 'check_xilinx') as xilinx, patch.object(doctor, 'check_docker') as docker:
            for target in ('web', 'server'):
                code, value = self.run_doctor(target)
                self.assertEqual(code, 0)
                self.assertTrue(value['ok'])
                self.assertEqual(value['target'], target)
            xilinx.assert_not_called()
            self.assertEqual(docker.call_count, 2)

    def test_image_requires_boot_tools(self):
        with patch.object(doctor, 'check_xilinx') as xilinx, patch.object(doctor, 'check_docker'):
            self.run_doctor('image')
            self.assertTrue(xilinx.call_args.kwargs['boot'])

    def test_standalone_doctor_resolves_arm64_configuration(self):
        stream = io.StringIO()
        with contextlib.redirect_stdout(stream), patch.object(doctor, 'check_docker') as docker:
            code = doctor.main(['--sdk-path', str(SDK), '--target', 'server',
                                '--cfg', 'examples/kria-kr260/test/config.mk', '--format', 'json'])
        self.assertEqual(code, 0)
        self.assertEqual(docker.call_args.kwargs['image'], 'cross-armhf:26.04')
        self.assertEqual(json.loads(stream.getvalue())['settings']['BOARD'], 'kria-kr260')

    def test_unreachable_docker_is_a_failure_with_recovery_command(self):
        reporter = Reporter(format='json')
        with patch.object(doctor.shutil, 'which', return_value='/usr/bin/docker'), \
             patch.object(doctor, 'command_output', return_value=(1, 'permission denied')):
            doctor.check_docker(reporter)
        self.assertEqual(reporter.errors, 1)
        self.assertIn('docker info', reporter.checks[-1]['remedy'])


class LocalCheckTest(unittest.TestCase):
    def test_malformed_settings_return_json_errors_without_running_checks(self):
        for data in ([], {}, {'settings': []}, {'settings': {'CONFIG_MK': '/config.mk'}},
                     {'settings': {'CONFIG_MK': '/config.mk', 'MEMORY_YML': 1}}):
            for command in (check.main, doctor.main):
                with self.subTest(data=data, command=command.__module__):
                    stream = io.StringIO()
                    with contextlib.redirect_stdout(stream), patch.object(sys, 'stdin', io.StringIO(json.dumps(data))), \
                         patch.object(check, 'run_check') as run:
                        code = command(['--sdk-path', str(SDK), '--format', 'json', '--settings-stdin'])
                    self.assertEqual(code, 1)
                    value = json.loads(stream.getvalue())
                    self.assertFalse(value['ok'])
                    self.assertIn('CONFIG_MK and MEMORY_YML', value['checks'][-1]['message'])
                    run.assert_not_called()

    def test_config_override_and_host_suites_do_not_call_hardware_tests(self):
        stream = io.StringIO()
        settings = {'CONFIG_MK': str(SDK / CFG), 'MEMORY_YML': '/selected/map.yml'}
        with contextlib.redirect_stdout(stream), patch.object(sys, 'stdin', io.StringIO(json.dumps({'settings': settings}))), \
             patch.object(check, 'run_check') as run:
            self.assertEqual(check.main(['--sdk-path', str(SDK), '--format', 'json', '--settings-stdin']), 0)
        value = json.loads(stream.getvalue())
        self.assertFalse(value['hardware_tests'])
        commands = [call.args[2] for call in run.call_args_list]
        self.assertIn('/selected/map.yml', commands[0])
        self.assertEqual(len(commands), 1 + len(check.SUITES))
        self.assertTrue(all('-m' in command and 'unittest' in command for command in commands[1:]))

    def test_failed_host_check_preserves_diagnostic_output(self):
        reporter = Reporter(format='json')
        with patch.dict(os.environ, {'CFG': CFG, 'HOST': 'example.invalid', 'TIMEOUT': '0.01'}):
            check.run_check(reporter, 'failing fixture', [sys.executable, '-c',
                'import os, sys; assert not any(key in os.environ for key in ("CFG", "HOST", "TIMEOUT")); '
                'print("specific failure", file=sys.stderr); sys.exit(9)'], SDK)
        self.assertEqual(reporter.errors, 1)
        self.assertEqual(reporter.checks[0]['output'], 'specific failure')

    def test_missing_dependencies_fail_without_installing_them(self):
        stream = io.StringIO()
        with contextlib.redirect_stdout(stream), patch.object(check.importlib.util, 'find_spec', return_value=None), \
             patch.object(check, 'run_check') as run:
            self.assertEqual(check.main(['--sdk-path', str(SDK), '--format', 'json']), 1)
        value = json.loads(stream.getvalue())
        self.assertFalse(value['ok'])
        self.assertTrue(all('make python_requirements' in item['remedy'] for item in value['checks']))
        run.assert_not_called()

    def test_host_report_includes_unittest_count(self):
        reporter = Reporter(format='json')
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / 'test_count.py').write_text('import unittest\nclass CountTest(unittest.TestCase):\n'
                                               '    def test_value(self): self.assertEqual(1 + 1, 2)\n')
            check.run_check(reporter, 'fixture suite', [sys.executable, '-m', 'unittest',
                            'discover', '-s', str(root)], SDK)
        self.assertEqual(reporter.errors, 0)
        self.assertEqual(reporter.checks[0]['tests'], 1)


if __name__ == '__main__':
    unittest.main()
