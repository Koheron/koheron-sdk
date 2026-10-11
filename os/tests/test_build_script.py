"""Check build.sh jobs and build-only behavior without running builds."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


SDK = Path(__file__).resolve().parents[2]


class BuildScriptTest(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.calls = self.root / 'calls'
        self.bin = self.root / 'bin'
        self.bin.mkdir()
        make = self.bin / 'make'
        make.write_text(f'''#!{sys.executable}
import json, pathlib, sys
with pathlib.Path({str(self.calls)!r}).open('a') as stream:
    stream.write(json.dumps(sys.argv[1:]) + '\\n')
''')
        make.chmod(0o755)
        nproc = self.bin / 'nproc'
        nproc.write_text('#!/bin/sh\nprintf "6\\n"\n')
        nproc.chmod(0o755)
        sudo = self.bin / 'sudo'
        sudo.write_text('#!/bin/sh\nexit 99\n')
        sudo.chmod(0o755)
        self.environment = {k: v for k, v in os.environ.items()
                            if k not in ('N_CPUS', 'MAKEFLAGS', 'MFLAGS')}
        self.environment['PATH'] = str(self.bin) + ':' + os.environ['PATH']

    def run_build(self, *args, **environment):
        return subprocess.run(['bash', str(SDK / 'build.sh'), *args], cwd=SDK,
                              text=True, input='', capture_output=True, timeout=10,
                              env={**self.environment, **environment})

    def assert_jobs(self, count):
        calls = [json.loads(line) for line in self.calls.read_text().splitlines()]
        self.assertTrue(calls)
        for args in calls:
            self.assertIn(f'-j{count}', args)
            self.assertIn(f'N_CPUS={count}', args)
            self.assertNotIn('-j', args)
        return calls

    def test_default_jobs_follow_available_cpu_count(self):
        result = self.run_build('red-pitaya')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        calls = self.assert_jobs(6)
        self.assertEqual(len(calls), 2)
        self.assertIn('COPY_INSTRUMENTS=phase-noise-analyzer', calls[-1])
        self.assertIn('image', calls[-1])
        self.assertFalse(any('flash' in args for args in calls))
        self.assertNotIn('burn the SD card', result.stdout + result.stderr)

    def test_override_applies_to_every_board_in_all_mode(self):
        result = self.run_build('--all', N_CPUS='3')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        calls = self.assert_jobs(3)
        expected = len(list((SDK / 'boards').glob('*/build_image.conf')))
        self.assertEqual(sum('image' in args for args in calls), expected)

    def test_invalid_job_limits_fail_before_building(self):
        for value in ('0', '-1', 'infinite', '1.5'):
            with self.subTest(value=value):
                result = self.run_build('red-pitaya', N_CPUS=value)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn('N_CPUS must be a positive integer', result.stderr)
                self.assertFalse(self.calls.exists())


if __name__ == '__main__':
    unittest.main()
