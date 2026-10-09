"""Temporary UIO sysfs/device roots and memory metadata for driver fixtures."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

def prepare_uio_fixture(root, header_source=None, runtime_root=None):
    root = Path(root)
    runtime_root = Path(runtime_root) if runtime_root else root
    (root / 'sys/uio0/maps/map0').mkdir(parents=True, exist_ok=True)
    (root / 'sys/uio0/maps/map0/addr').write_text('0x1000\n')
    (root / 'dev').mkdir(exist_ok=True)
    source = Path(header_source or ROOT / 'server/drivers/uio.hpp').read_text()
    assert source.count('"/sys/class/uio"') == 1
    assert source.count('fs::path("/dev")') == 1
    source = source.replace('"/sys/class/uio"', '"' + str(runtime_root / 'sys') + '"')
    source = source.replace('fs::path("/dev")', 'fs::path("' + str(runtime_root / 'dev') + '")')
    header = root / 'server/drivers/uio.hpp'
    header.parent.mkdir(parents=True, exist_ok=True)
    header.write_text(source)
    (root / 'memory.hpp').write_text('''#pragma once
#include <array>
#include <tuple>
#include <string_view>
constexpr auto memory_array = std::array{std::tuple{uintptr_t{0x1000}, uint32_t{4096},
uint32_t{3}, uint32_t{1}, std::string_view{"uio"}, std::string_view{"test"}}};
''')
