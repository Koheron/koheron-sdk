"""Compiler choices are independent of Vivado; builder overrides are explicit."""
from pathlib import Path
import subprocess
import unittest

SDK = Path(__file__).resolve().parents[2]


class CompilerSelectionTest(unittest.TestCase):
    def settings(self, board, *overrides):
        harness = '''include Makefile
.PHONY: compiler-selection-test
compiler-selection-test:
\t@printf '%s\\n' '$(GCC_VERSION)' '$(DOCKER_IMAGE)' '$(KERNEL_CC)' '$(UBOOT_CC)' '$(OS_HOSTCC)' '$(ATF_CC)'
'''
        result = subprocess.run(['make', '--no-print-directory', '-f', '-',
            'compiler-selection-test', f'CFG=examples/{board}/config.mk', *overrides],
            cwd=SDK, input=harness, text=True, capture_output=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return result.stdout.splitlines()

    def test_default_and_overridden_compilers_for_each_architecture(self):
        for board, prefix in [('alpha250/fft', 'arm-linux-gnueabihf'),
                              ('kria-kr260/test', 'aarch64-linux-gnu')]:
            for vivado in ['2025.1', '2026.1']:
                with self.subTest(board=board, vivado=vivado):
                    self.assertEqual(self.settings(board, f'VIVADO_VERSION={vivado}'),
                        ['15', 'cross-armhf:26.04', f'{prefix}-gcc-15',
                         f'{prefix}-gcc-15', 'gcc-15', f'{prefix}-gcc-15'])
                    self.assertEqual(self.settings(board, 'GCC_VERSION=13', f'VIVADO_VERSION={vivado}'),
                        ['13', 'cross-armhf:26.04', f'{prefix}-gcc-13',
                         f'{prefix}-gcc-13', 'gcc-13', f'{prefix}-gcc-13'])

    def test_kernel_and_boot_compilers_can_be_selected_separately(self):
        values = self.settings('alpha250/fft', 'KERNEL_GCC_VERSION=13',
                               'UBOOT_GCC_VERSION=13', 'HOST_GCC_VERSION=13')
        self.assertEqual(values[:2], ['15', 'cross-armhf:26.04'])
        self.assertEqual(values[2:5], ['arm-linux-gnueabihf-gcc-13',
                                      'arm-linux-gnueabihf-gcc-13', 'gcc-13'])


if __name__ == '__main__':
    unittest.main()
