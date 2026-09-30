"""Exercise the SDK's kernel dependency graph without compiling a kernel."""
from pathlib import Path
import os
import subprocess
import tempfile
import time
import unittest


SDK = Path(__file__).resolve().parents[2]


class LinuxMakeTest(unittest.TestCase):
    def exercise(self, arch, zynq_type, compiler, image):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            os_path = root / 'os'
            (os_path / 'patches/linux').mkdir(parents=True)
            (os_path / 'scripts').mkdir()
            for name in ('source-checksums.sha256', 'scripts/download_verified.sh'):
                (os_path / name).touch()
            kernel = root / f'linux-xlnx-{arch}-xilinx-linux-vtest'
            kernel.mkdir()
            archive = root / 'linux-xlnx-xilinx-linux-vtest.tar.gz'
            archive.touch()
            for name in ('.unpacked', '.patched'):
                (kernel / name).touch()
            source = os_path / f'xilinx_{zynq_type}_defconfig'
            source.write_text('CONFIG_FIRST=y\n')
            # Existing unpack/patch stamps keep this test offline.
            for path in (os_path / 'source-checksums.sha256',
                         os_path / 'scripts/download_verified.sh', archive,
                         kernel / '.unpacked', kernel / '.patched', source):
                os.utime(path, (100, 100))
            (kernel / 'Makefile').write_text('''\
SHELL := /bin/bash
.PHONY: xilinx_zynq_defconfig xilinx_zynqmp_defconfig zImage Image dtbs
xilinx_zynq_defconfig xilinx_zynqmp_defconfig:
\t@echo "configure $(ARCH) $(CROSS_COMPILE)" >> events
\t@cp arch/$(ARCH)/configs/$@ .config
\t@test ! -f fail-config
zImage Image:
\t@echo build >> events
dtbs:
\t@true
''')
            harness = root / 'Makefile'
            harness.write_text(f'''\
SHELL := /bin/bash
.SHELLFLAGS := -eu -o pipefail -c
.DELETE_ON_ERROR:
OS_PATH := {os_path}
TMP := {root}
ARCH := {arch}
ZYNQ_TYPE := {zynq_type}
GCC_ARCH := {compiler}
VIVADO_VERSION := test
KERNEL_BIN := {image}
N_CPUS := 1
DOCKER :=
ok =
include {SDK / 'os/linux.mk'}
''')
            config = kernel / '.config'
            stamp = kernel / '.built_all'

            def build(success=True):
                result = subprocess.run(['make', '--no-print-directory', '-f',
                                         str(harness), str(stamp)],
                                        text=True, capture_output=True)
                if success:
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                else:
                    self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
                return result

            def events():
                return (kernel / 'events').read_text().splitlines()

            def change_source(contents):
                source.write_text(contents)
                # Make the change strictly newer, even on coarse filesystems.
                previous = time.time() - 2
                os.utime(config, (previous, previous))
                os.utime(stamp, (previous, previous))

            build()
            self.assertEqual(config.read_text(), 'CONFIG_FIRST=y\n')
            self.assertEqual(events(), [f'configure {arch} {compiler}-', 'build'])
            timestamps = (config.stat().st_mtime_ns, stamp.stat().st_mtime_ns)
            build()
            self.assertEqual(len(events()), 2)
            self.assertEqual(timestamps, (config.stat().st_mtime_ns, stamp.stat().st_mtime_ns))

            change_source('CONFIG_SECOND=y\n')
            build()
            self.assertEqual(config.read_text(), 'CONFIG_SECOND=y\n')
            self.assertEqual(events().count('build'), 2)
            self.assertEqual(len(events()), 4)
            build()
            self.assertEqual(len(events()), 4)

            # Failed generation must not stamp success or run the kernel build.
            change_source('CONFIG_THIRD=y\n')
            (kernel / 'fail-config').touch()
            previous_stamp = stamp.stat().st_mtime_ns
            build(success=False)
            self.assertEqual(stamp.stat().st_mtime_ns, previous_stamp)
            self.assertEqual(events().count('build'), 2)
            self.assertFalse(config.exists())
            (kernel / 'fail-config').unlink()
            build()
            self.assertEqual(config.read_text(), 'CONFIG_THIRD=y\n')
            self.assertEqual(events().count('build'), 3)
            build()
            self.assertEqual(len(events()), 7)

    def test_arm_defconfig_updates(self):
        self.exercise('arm', 'zynq', 'arm-linux-gnueabihf', 'zImage')

    def test_arm64_defconfig_updates(self):
        self.exercise('arm64', 'zynqmp', 'aarch64-linux-gnu', 'Image')


if __name__ == '__main__':
    unittest.main()
