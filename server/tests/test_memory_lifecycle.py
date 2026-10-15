"""Shared memory ownership, fallback cleanup and partial initialization retries."""
from pathlib import Path
import tempfile
import unittest
from cpp_fixture import ROOT, compile_cpp, run_cpp
from uio_fixture import prepare_uio_fixture


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
        flags = ('-fno-exceptions', '-pthread', '-Wl,--wrap=open', '-Wl,--wrap=open64',
                 '-Wl,--wrap=close', '-Wl,--wrap=mmap', '-Wl,--wrap=mmap64', '-Wl,--wrap=munmap')
        compile_cpp(cls, 'server/tests/memory_lifecycle.cpp', flags=flags,
                    includes=(cls.root,), output=cls.binary)
        cls.reservation_binary = cls.root / 'reservation-test'
        compile_cpp(cls, 'server/tests/memory_lifecycle.cpp', flags=(*flags, '-DMEMORY_TEST_RESERVED'),
                    includes=(cls.root,), output=cls.reservation_binary)

    def test_cases(self):
        for case in (
            'checked_manager', 'checked_mapping', 'checked_open', 'checked_uio',
            'custom_fallback', 'custom', 'cloexec', 'mmap_failure', 'open_failure',
            'manager_retry', 'uio_fallback', 'reopen', 'uio', 'devmem',
        ):
            with self.subTest(case=case):
                run_cpp(self, self.binary, self.root, case)

    def test_reserved_dma_memory(self):
        run_cpp(self, self.reservation_binary, self.root, 'checked_reservation')


if __name__ == '__main__':
    unittest.main()
