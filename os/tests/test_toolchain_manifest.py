"""Check toolchain reporting and manifest staging without installed Xilinx tools."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest


SDK = Path(__file__).resolve().parents[2]
REPORTER = SDK / 'os/scripts/toolchain_versions.py'


class ToolchainManifestTest(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        # Deliberately omit release numbers from installation directory names.
        self.vivado = self.root / 'selected Vivado'
        self.vitis = self.root / 'selected Vitis'
        self.environment = dict(os.environ)
        self.environment.pop('BASH_ENV', None)

    def tool(self, path, product, version='2026.1', build='6511674',
             exit_code=0, malformed=False, stderr=False):
        executable = path / 'bin' / product
        executable.parent.mkdir(parents=True, exist_ok=True)
        option = '-version' if product == 'vivado' else '--version'
        banner = f'****** {product.title()} v{version} (64-bit)\n'
        if malformed:
            banner = 'Unexpected launcher output\n'
        elif build:
            banner += f'  **** SW Build {build} on 2026-06-16\n'
        executable.write_text(f'''\
#!/usr/bin/python3
import os, sys
assert sys.argv[1:] == [{option!r}]
assert 'BASH_ENV' not in os.environ
print({banner!r}, file={'sys.stderr' if stderr else 'sys.stdout'})
sys.exit({exit_code})
''')
        executable.chmod(0o755)

    @staticmethod
    def fields(text):
        return dict(line.split('=', 1) for line in text.splitlines())

    def report(self):
        # A bad SDK trap must not affect the vendor launchers.
        return subprocess.run(
            ['python3', str(REPORTER), '--vivado-path', str(self.vivado),
             '--vitis-path', str(self.vitis)], capture_output=True, text=True,
            env={**self.environment, 'BASH_ENV': '/unavailable-sdk-trap'}, timeout=5)

    def test_supported_releases_and_separate_installations(self):
        for release in ('2025.1', '2026.1.1'):
            with self.subTest(release=release):
                self.tool(self.vivado, 'vivado', release, '1234567')
                self.tool(self.vitis, 'vitis', '2026.1', '7654321', stderr=True)
                result = self.report()
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(self.fields(result.stdout), {
                    'vivado': release, 'vivado_build': '1234567',
                    'vitis': '2026.1', 'vitis_build': '7654321',
                })
                self.assertEqual(result.stderr, '')

    def test_unavailable_failed_and_malformed_queries_report_unknown(self):
        self.tool(self.vivado, 'vivado')
        for failure in ('missing', 'failed', 'malformed'):
            with self.subTest(failure=failure):
                if failure != 'missing':
                    self.tool(self.vitis, 'vitis', exit_code=7 if failure == 'failed' else 0,
                              malformed=failure == 'malformed')
                result = self.report()
                self.assertEqual(result.returncode, 0, result.stderr)
                values = self.fields(result.stdout)
                self.assertEqual(values['vivado'], '2026.1')
                self.assertEqual(values['vitis'], 'unknown')
                self.assertEqual(values['vitis_build'], 'unknown')
                self.assertIn('[WARN]', result.stderr)

    def test_missing_build_keeps_observed_release(self):
        self.tool(self.vivado, 'vivado', build=None)
        self.tool(self.vitis, 'vitis')
        result = self.report()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.fields(result.stdout)['vivado'], '2026.1')
        self.assertEqual(self.fields(result.stdout)['vivado_build'], 'unknown')
        self.assertIn('build number unavailable', result.stderr)

    def test_manifest_preserves_source_tags_and_stages_selected_tool_versions(self):
        self.tool(self.vivado, 'vivado')
        self.tool(self.vitis, 'vitis', '2025.1', '1234567')
        manifest = self.root / 'project/manifest-kria-kr260-test.txt'
        staged = self.root / 'os/rootfs_overlay/usr/local/share/koheron/manifest.txt'
        harness = f'''\
SHELL := /bin/bash
.SHELLFLAGS := -eu -o pipefail -c
OS_PATH := {SDK}/os
TMP_PROJECT_PATH := {self.root}/project
TMP_OS_PATH := {self.root}/os
VIVADO_PATH := {self.vivado}
VITIS_PATH := {self.vitis}
NAME := test
BOARD := kria-kr260
ZYNQ_TYPE := zynqmp
LINUX_TAG := xilinx-linux-v2025.1
UBOOT_TAG := xilinx-uboot-v2025.1
DTREE_TAG := xilinx_v2025.1
BUILD_ID := test-build
.PHONY: FORCE
FORCE:
%/:
\tmkdir -p $@
include {SDK}/os/rootfs.mk
'''

        def build():
            result = subprocess.run(
                ['make', '--no-print-directory', '-f', '-', str(staged)],
                input=harness, cwd=self.root, env=self.environment,
                capture_output=True, text=True, timeout=5)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual(manifest.read_bytes(), staged.read_bytes())
            return self.fields(manifest.read_text())

        values = build()
        self.assertEqual(values['kernel'], 'xilinx-linux-v2025.1')
        self.assertEqual(values['u-boot'], 'xilinx-uboot-v2025.1')
        self.assertEqual(values['device-tree'], 'xilinx_v2025.1')
        self.assertEqual(values['vivado'], '2026.1')
        self.assertEqual(values['vivado_build'], '6511674')
        self.assertEqual(values['vitis'], '2025.1')
        self.assertEqual(values['vitis_build'], '1234567')
        self.tool(self.vivado, 'vivado', '2026.1.1', '7654321')
        updated = build()
        self.assertEqual(updated['vivado'], '2026.1.1')
        self.assertEqual(updated['vivado_build'], '7654321')


if __name__ == '__main__':
    unittest.main()
