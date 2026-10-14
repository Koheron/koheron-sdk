"""Host test profiles select complete suites without board or synthesis tools."""
import contextlib
import io
import json
import os
from pathlib import Path
import sys
import unittest
from unittest.mock import patch

from koheron import check, test_profiles
from koheron.diagnostics import Reporter

SDK = Path(__file__).resolve().parents[2]


class CheckProfilesTest(unittest.TestCase):
    def test_auto_selects_native_tools_or_docker(self):
        with patch.object(test_profiles.shutil, 'which', side_effect=lambda name: '/bin/' + name):
            self.assertEqual(test_profiles.backend('auto', 'g++-15'), 'native')
        with patch.object(test_profiles.shutil, 'which', side_effect=lambda name: '/bin/docker' if name == 'docker' else None):
            self.assertEqual(test_profiles.backend('auto', 'g++-15'), 'docker')
            with self.assertRaisesRegex(ValueError, r'g\+\+-15 is unavailable'):
                test_profiles.backend('native', 'g++-15')
        with patch.object(test_profiles.shutil, 'which', return_value=None):
            with self.assertRaisesRegex(ValueError, 'docker is unavailable'):
                test_profiles.backend('docker', 'g++-15')

    def test_server_profile_discovers_compiled_suites_without_repeating_build_checks(self):
        with patch.object(test_profiles.shutil, 'which', return_value='/bin/docker'):
            suite = test_profiles.server_suite(SDK, 'docker', 'g++-15', 'selected-cpp')
        self.assertIn('test_cpp_client', suite.command)
        self.assertIn('test_fpga_manager', suite.command)
        self.assertNotIn('test_build_make', suite.command)
        self.assertIn('selected-cpp', suite.command)
        self.assertIn('--pull=never', suite.command)
        self.assertNotIn('--privileged', suite.command)
        self.assertEqual(suite.backend, 'docker')
        self.assertEqual(suite.env['CXX'], 'g++-15')

    def test_docker_paths_and_environment_remain_single_arguments(self):
        path = Path('/tmp/a workspace')
        command = test_profiles.docker_command(path, 'builder', ['python3', '-m', 'unittest'], {'CXX': 'g++-15'})
        self.assertIn(f'{path}:{path}', command)
        self.assertEqual(command[command.index('-w') + 1], str(path))
        self.assertIn('CXX=g++-15', command)
        self.assertEqual(command[-4:], ['builder', 'python3', '-m', 'unittest'])

    def test_sanitizers_and_browser_use_selected_compiler_and_images(self):
        with patch.object(test_profiles.shutil, 'which', return_value='/bin/docker'):
            suites = test_profiles.pna_suites(SDK, 'docker', 'g++-15', 'cpp-image', 'web-image')
            web = test_profiles.browser_suite(SDK, 'docker', 'web-image')
        self.assertEqual(suites[0].command[-2:], ['all', 'python'])
        self.assertEqual(suites[1].command[-2:], ['all', 'cpp'])
        self.assertEqual(suites[1].env['PNA_PYTHON'], sys.executable)
        self.assertEqual(suites[1].env['PNA_TEST_CXX'], 'g++-15')
        self.assertEqual(suites[1].env['PNA_CPP_IMAGE'], 'cpp-image')
        self.assertIn('web-image', web.command)
        self.assertEqual(web.command[-1], 'all')

    def test_temporary_fixtures_compile_and_execute_in_the_same_backend(self):
        directory = Path('/tmp/generated fixture')
        with patch.dict(os.environ, {'PNA_TEST_MODE': 'docker', 'PNA_CPP_IMAGE': 'selected-cpp'}):
            for command in (['g++-15', str(directory / 'probe.cpp')], [str(directory / 'probe')]):
                wrapped = test_profiles.fixture_command(SDK, directory, command)
                self.assertIn(f'{directory}:{directory}', wrapped)
                self.assertIn('selected-cpp', wrapped)
                self.assertEqual(wrapped[-len(command):], command)
        with patch.dict(os.environ, {'PNA_TEST_MODE': 'native'}):
            self.assertEqual(test_profiles.fixture_command(SDK, directory, ['probe']), ['probe'])

    def test_full_profile_executes_every_family_and_reports_backends(self):
        output = io.StringIO()

        def success(reporter, label, command, sdk_path, **kwargs):
            return reporter.ok(label)

        with contextlib.redirect_stdout(output), patch.object(check, 'find_configs', return_value=[]), \
             patch.object(test_profiles.shutil, 'which', return_value='/bin/docker'), \
             patch.object(check, 'run_check', side_effect=success) as run:
            # Supply one resolved configuration so this test never invokes Make or a board.
            with patch.object(sys, 'stdin', io.StringIO(json.dumps({'settings': {
                'CONFIG_MK': '/config.mk', 'MEMORY_YML': '/memory.yml'}}))):
                result = check.main(['--sdk-path', str(SDK), '--profile', 'full', '--mode', 'docker',
                                     '--format', 'json', '--settings-stdin'])
        self.assertEqual(result, 0)
        report = json.loads(output.getvalue())
        self.assertEqual(report['profile'], 'full')
        self.assertFalse(report['hardware_tests'])
        self.assertEqual(run.call_count, 1 + len(check.SUITES) + 4)
        labels = [entry['message'] for entry in report['checks']]
        self.assertIn('Server C++ tests', labels)
        self.assertIn('Browser tests', labels)
        self.assertTrue(any('ASan/UBSan' in label for label in labels))

    def test_missing_native_tools_fail_full_profile_without_running_partial_compiled_suites(self):
        output = io.StringIO()
        with contextlib.redirect_stdout(output), patch.object(check, 'find_configs', return_value=[Path('/config.mk')]), \
             patch.object(test_profiles.shutil, 'which', side_effect=lambda name: '/bin/tclsh' if name == 'tclsh' else None), \
             patch.object(check, 'run_check', side_effect=lambda reporter, label, *args, **kwargs: reporter.ok(label)) as run:
            self.assertEqual(check.main(['--sdk-path', str(SDK), '--profile', 'full',
                                         '--mode', 'native', '--format', 'json']), 1)
        self.assertEqual(run.call_count, 1 + len(check.SUITES))
        self.assertFalse(json.loads(output.getvalue())['ok'])

    def test_combined_unittest_and_node_counts_are_added(self):
        reporter = Reporter(format='json')
        check.run_check(reporter, 'multiple suites', [sys.executable, '-c',
                        'print("Ran 2 tests in 0.1s\\nRan 3 tests in 0.2s\\n# tests 4\\nℹ tests 5")'], SDK)
        self.assertEqual(reporter.checks[0]['tests'], 14)

    def test_suite_environment_overrides_reach_children(self):
        reporter = Reporter(format='json')
        check.run_check(reporter, 'compiler setting', [sys.executable, '-c',
                        'import os; assert os.environ["CXX"] == "chosen-compiler"'], SDK,
                        env_updates={'CXX': 'chosen-compiler'})
        self.assertEqual(reporter.errors, 0)


if __name__ == '__main__':
    unittest.main()
