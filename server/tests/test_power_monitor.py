"""INA230 bounds, signed conversion and I2C failures using simulated registers."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]

class PowerMonitorTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.directory.cleanup)
        cls.binary = Path(cls.directory.name) / 'power-monitor-test'
        command = [os.environ.get('CXX', 'g++'), '-std=c++23', '-O2', '-Wall',
                   '-Wextra', '-Werror', '-fno-exceptions', '-pthread', '-I', str(ROOT)]
        command += shlex.split(os.environ.get('CXXFLAGS', ''))
        command += [str(ROOT / 'server/tests/power_monitor.cpp'),
                    str(ROOT / 'boards/alpha250/drivers/power-monitor.cpp'), '-o', str(cls.binary)]
        subprocess.run(command, check=True, timeout=90)

    def run_case(self, case):
        result = subprocess.run([str(self.binary), case], capture_output=True,
                                text=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_invalid_indices_do_not_access_bus(self):
        self.run_case('bounds')

    def test_every_signed_shunt_code_on_both_supplies(self):
        self.run_case('shunt')

    def test_every_bus_code_on_both_supplies(self):
        self.run_case('bus')

    def test_ui_order_and_current_scaling(self):
        self.run_case('ui')

    def test_register_selection_failure_stops_read(self):
        self.run_case('write_failure')

    def test_partial_destination_on_read_failure_is_not_used(self):
        self.run_case('read_failure')

if __name__ == '__main__':
    unittest.main()
