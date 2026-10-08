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
.PHONY: xilinx_zynq_defconfig xilinx_zynqmp_defconfig scripts_dtc zImage Image dtbs
xilinx_zynq_defconfig xilinx_zynqmp_defconfig:
\t@echo "configure $(ARCH) $(CROSS_COMPILE)" >> events
\t@cp arch/$(ARCH)/configs/$@ .config
\t@test ! -f fail-config
scripts_dtc:
\t@echo dtc >> events
\t@test ! -f fail-dtc
\t@mkdir -p scripts/dtc && touch scripts/dtc/dtc && chmod +x scripts/dtc/dtc
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
LINUX_VERSION := test
KERNEL_BIN := {image}
N_CPUS := 1
DOCKER :=
ok =
include {SDK / 'os/linux.mk'}
''')
            config = kernel / '.config'
            stamp = kernel / '.built_all'
            dtc = kernel / 'scripts/dtc/dtc'

            def build(success=True, target=stamp, extra_args=()):
                result = subprocess.run(['make', '-j4', '--no-print-directory', '-f',
                                         str(harness), str(target), *extra_args],
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

            build(target=dtc)
            self.assertEqual(events(), [f'configure {arch} {compiler}-', 'dtc'])
            self.assertFalse(stamp.exists())
            build(target=dtc)
            self.assertEqual(len(events()), 2)
            build()
            self.assertEqual(config.read_text(), 'CONFIG_FIRST=y\n')
            self.assertEqual(events(), [f'configure {arch} {compiler}-', 'dtc', 'build'])
            timestamps = (config.stat().st_mtime_ns, stamp.stat().st_mtime_ns)
            build()
            self.assertEqual(len(events()), 3)
            self.assertEqual(timestamps, (config.stat().st_mtime_ns, stamp.stat().st_mtime_ns))

            change_source('CONFIG_SECOND=y\n')
            build()
            self.assertEqual(config.read_text(), 'CONFIG_SECOND=y\n')
            self.assertEqual(events().count('build'), 2)
            self.assertEqual(len(events()), 6)
            build()
            self.assertEqual(len(events()), 6)

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
            self.assertEqual(len(events()), 10)

            # A missing compiler must be rebuilt before the kernel; failure
            # must leave the previous kernel success stamp untouched.
            dtc.unlink()
            (kernel / 'fail-dtc').touch()
            previous_stamp = stamp.stat().st_mtime_ns
            build(success=False)
            self.assertFalse(dtc.exists())
            self.assertEqual(stamp.stat().st_mtime_ns, previous_stamp)
            self.assertEqual(events().count('build'), 3)
            (kernel / 'fail-dtc').unlink()
            build()
            self.assertTrue(dtc.exists())
            self.assertEqual(events().count('build'), 4)

            # Changing compiler selection must rebuild an already cached kernel.
            build(extra_args=['KERNEL_GCC_VERSION=16'])
            self.assertEqual(events().count('build'), 5)
            compiler_stamp = kernel / '.compiler-settings'
            self.assertIn(f'CC={compiler}-gcc-16', compiler_stamp.read_text())
            previous_events = events()
            previous_time = compiler_stamp.stat().st_mtime_ns
            build(extra_args=['KERNEL_GCC_VERSION=16'])
            self.assertEqual(events(), previous_events)
            self.assertEqual(compiler_stamp.stat().st_mtime_ns, previous_time)

    def test_arm_defconfig_updates(self):
        self.exercise('arm', 'zynq', 'arm-linux-gnueabihf', 'zImage')

    def test_arm64_defconfig_updates(self):
        self.exercise('arm64', 'zynqmp', 'aarch64-linux-gnu', 'Image')


if __name__ == '__main__':
    unittest.main()
