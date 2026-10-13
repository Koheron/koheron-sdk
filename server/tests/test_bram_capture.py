"""Production BRAM driver: fresh captures, manual requests and missing completion."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[2]
class BramCaptureTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        directory=tempfile.TemporaryDirectory();cls.addClassCleanup(directory.cleanup)
        cls.binary=Path(directory.name)/'bram-capture'
        command=[os.environ.get('CXX','g++'),'-std=c++23','-O2','-Wall','-Wextra','-Werror','-pthread','-fno-exceptions',
                 '-I',str(ROOT/'server/tests/fixtures/bram_capture'),'-I',str(ROOT)]
        command+=shlex.split(os.environ.get('CXXFLAGS',''))
        subprocess.run(command+[str(ROOT/'server/tests/bram_capture.cpp'),'-o',str(cls.binary)],check=True,timeout=90)
    def run_case(self,case):
        result=subprocess.run([str(self.binary),case],capture_output=True,text=True,timeout=5)
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)
    def test_fresh_capture_both_completion_values(self):self.run_case('fresh')
    def test_manual_trigger_finishes_before_next_capture(self):self.run_case('manual')
    def test_missing_completion_never_reads_buffer(self):self.run_case('timeout')
if __name__=='__main__':unittest.main()
