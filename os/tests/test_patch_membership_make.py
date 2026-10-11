"""Removed source overlays must disappear from cached OS builds."""
from pathlib import Path
import subprocess
import tarfile
import tempfile
import unittest


SDK = Path(__file__).resolve().parents[2]


class PatchMembershipMakeTest(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.os_path = self.root / 'os-source'
        (self.os_path / 'scripts').mkdir(parents=True)
        (self.os_path / 'source-checksums.sha256').touch()
        downloader = self.os_path / 'scripts/download_verified.sh'
        downloader.write_text('#!/bin/sh\nexit 0\n')

    def archive(self, name, files):
        source = self.root / 'source'
        for path, contents in files.items():
            target = source / path
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text(contents)
        archive = self.root / name
        with tarfile.open(archive, 'w:gz') as stream:
            stream.add(source, arcname='source')
        return archive

    def make(self, harness, target, *assignments):
        result = subprocess.run(
            ['make', '--no-print-directory', '-j4', '-f', '-', str(target), *assignments],
            input=harness, cwd=SDK, text=True, capture_output=True, timeout=15)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_linux_patch_removal_restores_vendor_source(self):
        patch = self.os_path / 'patches/linux/drivers/example.c'
        patch.parent.mkdir(parents=True)
        patch.write_text('patched\n')
        archive = self.archive('linux.tar.gz', {
            'drivers/example.c': 'vendor\n',
            'drivers/Makefile': 'obj-y += base/\n',
        })
        kernel = self.root / 'linux-xlnx-arm-xilinx-linux-vtest'
        harness = f'''SHELL := /bin/bash
.SHELLFLAGS := -eu -o pipefail -c
.DELETE_ON_ERROR:
OS_PATH := {self.os_path}
TMP := {self.root}
ARCH := arm
ZYNQ_TYPE := zynq
GCC_ARCH := arm-linux-gnueabihf
LINUX_VERSION := test
SOURCE_CHECKSUMS := {self.os_path / 'source-checksums.sha256'}
DOWNLOAD_VERIFIED := {self.os_path / 'scripts/download_verified.sh'}
%/:
\tmkdir -p $@
include {SDK / 'os/linux.mk'}
'''
        self.make(harness, kernel / '.patched', f'LINUX_TAR={archive}')
        self.assertEqual((kernel / 'drivers/example.c').read_text(), 'patched\n')
        patch.unlink()
        self.make(harness, kernel / '.patched', f'LINUX_TAR={archive}')
        self.assertEqual((kernel / 'drivers/example.c').read_text(), 'vendor\n')

    def test_uboot_patch_removal_restores_vendor_source(self):
        patch = self.root / 'patches/u-boot/drivers/example.c'
        patch.parent.mkdir(parents=True)
        patch.write_text('patched\n')
        patch_script = self.os_path / 'scripts/patch_uboot.sh'
        patch_script.write_text('#!/bin/sh\nexit 0\n')
        archive = self.archive('uboot.tar.gz', {
            'drivers/example.c': 'vendor\n',
            'Makefile': '.PHONY: mrproper demo_defconfig\nmrproper:\n\trm -f .config\n'
                        'demo_defconfig:\n\t@printf "configured\\n" > .config\n',
        })
        uboot = self.root / 'uboot'
        harness = f'''SHELL := /bin/bash
.SHELLFLAGS := -eu -o pipefail -c
.DELETE_ON_ERROR:
OS_PATH := {self.os_path}
TMP := {self.root}
TMP_PROJECT_PATH := {self.root / 'project'}
TMP_OS_PATH := {self.root / 'os-build'}
TMP_OS_BOARD_PATH := {self.root / 'board'}
PATCHES := {self.root / 'patches'}
UBOOT_CONFIG := demo_defconfig
GCC_ARCH := arm-linux-gnueabihf
KERNEL_BIN := zImage
DOCKER :=
SOURCE_CHECKSUMS := {self.os_path / 'source-checksums.sha256'}
DOWNLOAD_VERIFIED := {self.os_path / 'scripts/download_verified.sh'}
%/:
\tmkdir -p $@
include {SDK / 'os/os.mk'}
'''
        assignments = (f'UBOOT_TAR={archive}', f'UBOOT_PATH={uboot}')
        self.make(harness, uboot / '.config', *assignments)
        self.assertEqual((uboot / 'drivers/example.c').read_text(), 'patched\n')
        patch.unlink()
        self.make(harness, uboot / '.config', *assignments)
        self.assertEqual((uboot / 'drivers/example.c').read_text(), 'vendor\n')


if __name__ == '__main__':
    unittest.main()
