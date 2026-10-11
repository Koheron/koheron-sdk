"""Release/configuration changes must replace even a newer cached output ELF."""
from pathlib import Path
import subprocess
import tempfile
import unittest

SDK = Path(__file__).resolve().parents[2]


class UBootMakeTest(unittest.TestCase):
    def test_cached_release_and_configuration_switches_rebuild_shared_output(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for name in ('archive', 'checksums', 'download'):
                (root / name).touch()
            patch_selection = root / 'board/.uboot-patches'
            patch_selection.parent.mkdir()
            patch_selection.write_text('\n')
            for version in ('2025.1', '2026.1'):
                source = root / version
                source.mkdir()
                (source / '.unpacked').touch()
                (source / 'Makefile').write_text(f'''\
.PHONY: mrproper all
mrproper:
\trm -f .config
%_defconfig:
\t@printf '%s\\n' '$@' > .config
all:
\t@printf '%s %s\\n' '{version}' "$$(cat .config)" > u-boot
\t@echo build >> {root}/events
''')
            harness = f'''\
SHELL := /bin/bash
.SHELLFLAGS := -eu -o pipefail -c
.DELETE_ON_ERROR:
OS_PATH := {SDK}/os
TMP_PROJECT_PATH := {root}
TMP_OS_PATH := {root}/os
TMP_OS_BOARD_PATH := {root}/board
KERNEL_BIN := zImage
UBOOT_PATH = {root}/$(UBOOT_VERSION)
GCC_ARCH := arm-linux-gnueabihf
N_CPUS := 2
SOURCE_CHECKSUMS := {root}/checksums
DOWNLOAD_VERIFIED := {root}/download
%/:
\tmkdir -p $@
include {SDK}/os/os.mk
'''

            def build(version, config='first_defconfig'):
                result = subprocess.run(
                    ['make', '--no-print-directory', '-f', '-', 'uboot',
                     f'UBOOT_VERSION={version}', f'UBOOT_CONFIG={config}',
                     f'UBOOT_TAR={root}/archive'], input=harness, cwd=SDK,
                    text=True, capture_output=True, timeout=10)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertEqual((root / 'board/u-boot.elf').read_text(), f'{version} {config}\n')
                self.assertFalse((root / 'os/.linux-selection').exists())

            build('2025.1')
            build('2026.1')
            build('2025.1')
            build('2025.1', 'second_defconfig')
            events = (root / 'events').read_text()
            self.assertEqual(events, 'build\n' * 4)
            build('2025.1', 'second_defconfig')
            self.assertEqual((root / 'events').read_text(), events)


if __name__ == '__main__':
    unittest.main()
