"""Shared memory ownership, fallback cleanup and partial initialization retries."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest
from uio_fixture import prepare_uio_fixture

ROOT = Path(__file__).resolve().parents[2]


def prepare_memory_fixture(root, runtime_root=None):
    prepare_uio_fixture(root, runtime_root=runtime_root)
    (Path(root) / 'memory.hpp').write_text('''#pragma once
#include <sys/mman.h>
#include <array>
#include <cstdint>
#include <tuple>
#include <string_view>
namespace mem {
inline constexpr std::size_t count = 4;
constexpr std::array<std::tuple<uintptr_t, uint32_t, uint32_t, uint32_t,
                                std::string_view, std::string_view>, count> memory_array{{
    {0x1080, 4096, 3, 1, "/dev/mem", "physical"},
    {0x1080, 4096, 3, 1, "/fixture/custom", "custom"},
    {0x1000, 4096, 3, 1, "/dev/uio", "uio"},
    {0x3000, 4096, 3, 1, "/dev/uio", "fallback"}
}};
}
''')


class MemoryLifecycleTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.directory.cleanup)
        cls.root = Path(cls.directory.name)
        prepare_memory_fixture(cls.root)
        cls.binary = cls.root / 'memory-test'
        command = [os.environ.get('CXX', 'g++'), '-std=c++23', '-O2', '-Wall',
                   '-Wextra', '-Werror', '-fno-exceptions', '-pthread',
                   '-I', str(cls.root), '-I', str(ROOT)]
        command += shlex.split(os.environ.get('CXXFLAGS', ''))
        command += [str(ROOT / 'server/tests/memory_lifecycle.cpp'),
                    '-Wl,--wrap=open', '-Wl,--wrap=open64', '-Wl,--wrap=close',
                    '-Wl,--wrap=mmap', '-Wl,--wrap=mmap64', '-Wl,--wrap=munmap', '-o', str(cls.binary)]
        subprocess.run(command, check=True, timeout=90)

    def run_case(self, case):
        result = subprocess.run([str(self.binary), str(self.root), case],
                                capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_unaligned_physical_mapping(self): self.run_case('devmem')
    def test_custom_device_uses_exact_mapping_length(self): self.run_case('custom')
    def test_uio_owns_mapping_and_descriptor(self): self.run_case('uio')
    def test_missing_uio_fallback_releases_resources(self): self.run_case('uio_fallback')
    def test_custom_device_fallback_releases_resources(self): self.run_case('custom_fallback')
    def test_failed_mapping_closes_descriptor_and_allows_retry(self): self.run_case('mmap_failure')
    def test_failed_open_leaves_empty_state_and_allows_retry(self): self.run_case('open_failure')
    def test_repeated_open_preserves_mapping_and_contents(self): self.run_case('reopen')
    def test_descriptors_are_not_inherited_across_exec(self): self.run_case('cloexec')
    def test_manager_retries_only_failed_maps(self): self.run_case('manager_retry')


if __name__ == '__main__':
    unittest.main()
