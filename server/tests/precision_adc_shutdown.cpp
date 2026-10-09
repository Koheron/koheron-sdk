#include "boards/alpha250/drivers/precision-adc.hpp"
#include "server/hardware/spi_manager.hpp"
#include "server/runtime/services.hpp"
#include <cassert>
#include <atomic>
#include <cstdarg>
#include <cstring>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <chrono>

namespace {
std::atomic<unsigned> reads{0};
bool deny_open = false;
std::atomic<bool> fail_read{false};
}
extern "C" int __real_open(const char*, int, ...);
extern "C" int __wrap_open(const char*, int flags, ...) {
    if (deny_open) { errno = EACCES; return -1; }
    return __real_open("/dev/null", flags);
}
extern "C" int __wrap_open64(const char* path, int flags, ...) {
    return __wrap_open(path, flags);
}
extern "C" int __wrap_ioctl(int, unsigned long request, ...) {
    if (request != SPI_IOC_MESSAGE(1)) { return 0; }
    va_list args;
    va_start(args, request);
    auto* transfer = va_arg(args, spi_ioc_transfer*);
    va_end(args);
    if (transfer->len == 5) {
        ++reads;
        if (fail_read) { errno = EIO; return -1; }
    }
    if (transfer->rx_buf) {
        std::memset(reinterpret_cast<void*>(transfer->rx_buf), 0, transfer->len);
    }
    return 0;
}
int main() {
    auto spi = services::provide<hw::SpiManager>();
    assert(spi->init() == 0);
    deny_open = true;
    { PrecisionAdc unavailable; }
    deny_open = false;
    // Immediate destruction exercises a worker not yet scheduled by the OS.
    for (unsigned i = 0; i < 100; ++i) { PrecisionAdc adc; }
    for (bool fail : {false, true}) {
        fail_read = fail;
        reads = 0;
        {
            PrecisionAdc adc;
            while (reads == 0) { std::this_thread::yield(); }
            adc.get_adc_values();
        }
        const auto stopped_reads = reads.load();
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        assert(reads == stopped_reads);
    }
    services::remove<hw::SpiManager>();
}
