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
    root = Path(root)
    runtime_root = Path(runtime_root) if runtime_root else root
    source = (ROOT / 'server/hardware/memory_map.hpp').read_text()
    source = source.replace('"/proc/iomem"', '"' + str(runtime_root / 'iomem') + '"')
    (root / 'server/hardware').mkdir(parents=True, exist_ok=True)
    (root / 'server/hardware/memory_map.hpp').write_text(source)
    (Path(root) / 'memory.hpp').write_text('''#pragma once
#include <sys/mman.h>
#include <array>
#include <cstdint>
#include <tuple>
#include <string_view>
namespace mem {
inline constexpr std::size_t count = 4;
#ifdef MEMORY_TEST_RESERVED
inline constexpr std::string_view physical_device = "/dev/mem_wc";
#else
inline constexpr std::string_view physical_device = "/dev/mem";
#endif
constexpr std::array<std::tuple<uintptr_t, uint32_t, uint32_t, uint32_t,
                                std::string_view, std::string_view>, count> memory_array{{
    {0x1080, 4096, 3, 1, physical_device, "physical"},
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
                    '-Wl,--wrap=mmap', '-Wl,--wrap=mmap64', '-Wl,--wrap=munmap']
        subprocess.run(command + ['-o', str(cls.binary)], check=True, timeout=90)
        cls.reservation_binary = cls.root / 'reservation-test'
        subprocess.run(command + ['-DMEMORY_TEST_RESERVED', '-o', str(cls.reservation_binary)],
                       check=True, timeout=90)

    def run_case(self, case, *, binary=None):
        result = subprocess.run([str(binary or self.binary), str(self.root), case],
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
    def test_checked_mapping_errors_survive_cleanup_and_retry(self): self.run_case('checked_mapping')
    def test_checked_open_and_fallback_errors_are_distinct(self): self.run_case('checked_open')
    def test_checked_manager_returns_all_failures_and_preserves_successful_maps(self): self.run_case('checked_manager')
    def test_checked_uio_discovery_errors_and_invalid_descriptor(self): self.run_case('checked_uio')
    def test_checked_dma_reservation_errors_block_device_access(self):
        self.run_case('checked_reservation', binary=self.reservation_binary)


if __name__ == '__main__':
    unittest.main()
