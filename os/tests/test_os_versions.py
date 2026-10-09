"""Check runtime release selection independently of the installed FPGA tools."""
from pathlib import Path
import subprocess
import unittest


SDK = Path(__file__).resolve().parents[2]


class OsVersionsTest(unittest.TestCase):
    def versions(self, board, vivado, *overrides):
        harness = '''include Makefile
.PHONY: print-os-versions
print-os-versions:
\t@printf '%s\\n' '$(LINUX_TAG)' '$(LINUX_URL)' '$(UBUNTU_VERSION)' '$(UBOOT_TAG)' '$(DTREE_TAG)' '$(ATRUST_TAG)' '$(VIVADO_PATH)' '$(UBUNTU_ARCH)'
'''
        result = subprocess.run(
            ['make', '--no-print-directory', '-f', '-', 'print-os-versions',
             f'CFG=examples/{board}/config.mk', f'VIVADO_VERSION={vivado}', *overrides],
            input=harness, cwd=SDK, capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return result.stdout.splitlines()

    def test_latest_runtime_is_independent_of_supported_toolchain(self):
        for board, arch in (('alpha250/fft', 'armhf'), ('kria-kr260/test', 'arm64')):
            for vivado in ('2025.1', '2025.2', '2026.1'):
                with self.subTest(board=board, vivado=vivado):
                    values = self.versions(board, vivado)
                    self.assertEqual(values[:3], [
                        'xilinx-linux-v2026.1',
                        'https://github.com/Xilinx/linux-xlnx/archive/refs/tags/xilinx-v2026.1.tar.gz',
                        '26.04.1'])
                    self.assertEqual(values[3:5], [f'xilinx-uboot-v{vivado}', f'xilinx_v{vivado}'])
                    self.assertEqual(values[5], f'arm-trust-v{vivado}' if arch == 'arm64' else '')
                    self.assertEqual(values[6:], [f'/tools/Xilinx/{vivado}/Vivado', arch])
                    checksums = (SDK / 'os/source-checksums.sha256').read_text()
                    for archive in (f'u-boot-xlnx-xilinx-uboot-v{vivado}.tar.gz',
                                    f'device-tree-xlnx-xilinx_v{vivado}.tar.gz',
                                    f'arm-trust-xlnx-arm-trust-v{vivado}.tar.gz'):
                        self.assertIn(archive, checksums)

    def test_older_runtime_can_be_selected_with_newer_tools(self):
        values = self.versions('alpha250/fft', '2026.1',
                               'LINUX_VERSION=2025.1', 'UBUNTU_VERSION=24.04.5')
        self.assertEqual(values[0], 'xilinx-linux-v2025.1')
        self.assertIn('xilinx-v2025.1.tar.gz', values[1])
        self.assertEqual(values[2:5], ['24.04.5', 'xilinx-uboot-v2026.1', 'xilinx_v2026.1'])


if __name__ == '__main__':
    unittest.main()
