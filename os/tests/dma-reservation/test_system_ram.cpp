#include "server/hardware/system_ram.hpp"
#include <cassert>
#include <sstream>
#include <iostream>

int main() {
    const auto safe = [](const char* text, uint64_t base = 0x18000000,
                         uint64_t size = 0x08000000) {
        std::istringstream input{text};
        return hw::outside_system_ram(input, base, size);
    };
    assert(safe("00000000-17ffffff : System RAM\n"));
    assert(!safe("00000000-1fffffff : System RAM\n")); // reusable CMA
    assert(!safe("00000000-18000000 : System RAM\n")); // one-byte overlap
    assert(!safe("1fffffff-2fffffff : System RAM\n"));
    assert(safe("20000000-2fffffff : System RAM\n"));
    assert(!safe("00000000-17ffffff : System RAM\n19000000-19ffffff : System RAM\n"));
    assert(safe("00000000-17ffffff : System RAM\n  00008000-00afffff : Kernel code\n"
                "18000000-1fffffff : reserved\n80000000-8000ffff : dma\n"));
    assert(safe("  00000000-17ffffff  :\tSystem RAM \r\n"));
    assert(!safe(""));
    assert(!safe("00000000-00000000 : System RAM\n")); // redacted addresses
    assert(!safe("00000000-nope : System RAM\n"));
    assert(!safe("00000000-17ffffffjunk : System RAM\n"));
    assert(!safe("1fffffff-00000000 : System RAM\n"));
    assert(!safe("00000000-17ffffff : System RAM\n", UINT64_MAX, 2));
    assert(!safe("00000000-17ffffff : System RAM\n", 0x18000000, 0));
    assert(safe("0000000100000000-00000001ffffffff : System RAM\n"));
    assert(!safe("0000000100000000-00000001ffffffff : System RAM\n", 0x100000000, 0x1000));
    std::cout << "Fixed DMA System RAM exclusion checks passed\n";
}
