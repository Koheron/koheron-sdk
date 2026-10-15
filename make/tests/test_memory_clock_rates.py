"""Compile requested fabric-clock rates over rounded hardware-export values."""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

SDK = Path(__file__).resolve().parents[2]


@unittest.skipUnless(shutil.which('dtc') and shutil.which('fdtget'), 'requires dtc and fdtget')
class MemoryClockRateTest(unittest.TestCase):
    def test_overlay_uses_requested_rates_and_only_declared_clocks(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            config = root / 'memory.yml'
            config.write_text('memory: []\nparameters:\n  fclk0: 200000000\n  fclk1: 143000000\n')
            generated = root / 'memory.dtsi'
            subprocess.run([sys.executable, str(SDK / 'make.py'), '--memory_dtsi',
                            str(generated), str(config)], check=True,
                           env={**os.environ, 'SDK_PATH': str(SDK), 'ARCH': 'arm64'}, timeout=10)
            source = root / 'clocks.dts'
            source.write_text('''/dts-v1/;
/ {
    amba: amba {};
    clocking0: clocking0 { assigned-clock-rates = <199998000>; };
    clocking1: clocking1 { assigned-clock-rates = <142855714>; };
};
/include/ "memory.dtsi"
''')
            output = root / 'clocks.dtb'
            subprocess.run(['dtc', '-I', 'dts', '-O', 'dtb', '-o', str(output), str(source)],
                           check=True, capture_output=True, timeout=10)
            for index, expected in enumerate(('200000000', '143000000')):
                rate = subprocess.check_output(['fdtget', '-t', 'u', str(output),
                                                f'/clocking{index}', 'assigned-clock-rates'], text=True)
                self.assertEqual(rate.strip(), expected)
            self.assertNotIn('&clocking2', generated.read_text())
            self.assertNotIn('&clocking3', generated.read_text())
