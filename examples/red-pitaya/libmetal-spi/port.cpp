#include "port/xil_io.h"
#include "port/xil_util.h"
#include <xil_assert.h>
#include <xstatus.h>
#include <metal/io.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

namespace {
// One controller per process in this example. Bind/unbind and driver calls are
// serialized against libmetal's interrupt worker by metal_irq_save_disable().
metal_io_region* region = nullptr;

unsigned long offset_of(UINTPTR address) {
    if (region != nullptr) {
        const auto base = reinterpret_cast<UINTPTR>(metal_io_virt(region, 0));
        const auto size = metal_io_region_size(region);
        if (size >= sizeof(u32) && address >= base &&
            address - base <= size - sizeof(u32) && (address - base) % sizeof(u32) == 0) {
            return address - base;
        }
    }
    std::fputs("XSpi: invalid libmetal register access\n", stderr);
    std::abort();
}
} // namespace

extern "C" {
u32 Xil_AssertStatus = 0;
s32 Xil_AssertWait = 0;

void Xil_Assert(const char8* file, s32 line) {
    std::fprintf(stderr, "XSpi assertion: %s:%d\n", file, line);
    std::abort();
}

void spi_port_bind(metal_io_region* io) {
    region = io;
}

u32 Xil_In32(UINTPTR address) {
    return metal_io_read32(region, offset_of(address));
}

void Xil_Out32(UINTPTR address, u32 value) {
    metal_io_write32(region, offset_of(address), value);
}

s32 Xil_WaitForEvent(UINTPTR address, u32 mask, u32 expected, u32 timeout_us) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::microseconds(timeout_us);
    do {
        if ((Xil_In32(address) & mask) == expected) {
            return XST_SUCCESS;
        }
        std::this_thread::sleep_for(std::chrono::microseconds(1));
    } while (std::chrono::steady_clock::now() < deadline);
    return XST_FAILURE;
}
} // extern "C"
