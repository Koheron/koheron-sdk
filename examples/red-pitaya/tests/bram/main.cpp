#include "server/runtime/syslog.hpp"
#include "server/runtime/systemd.hpp"
#include "server/hardware/memory_manager.hpp"
#include "server/hardware/fpga_manager.hpp"
#include "server/hardware/zynq_fclk.hpp"

#include <array>
#include <chrono>

#define DO_NOT_OPTIMIZE(x) asm volatile("" : : "r"(x) : "memory")

int main() {
    hw::FpgaManager fpga;
    hw::ZynqFclk fclk;
    hw::MemoryManager mm;

    if (fpga.load_bitstream() < 0) {
        log<PANIC>("Failed to load bitstream.\n");
        return -1;
    }

    if (mm.open() < 0) {
        log<PANIC>("Failed to open memory");
        return -1;
    }

    fclk.set("fclk0", 187500000);
    rt::systemd::notify_ready();

    auto& bram0 = mm.get<mem::bram0>();
    auto& bram1 = mm.get<mem::bram1>();
    constexpr uint32_t bram_size = mem::bram0_range / sizeof(uint32_t);

    // 1) Initialize
    std::array<uint32_t, bram_size> data;
    for (uint32_t i = 0; i < data.size(); ++i) {
        data[i] = 0x12340000u + i;
    }

    auto fence = [](){ asm volatile("" ::: "memory"); };

    // Warmup (touch paths & TLB)
    bram0.write_array<uint32_t, bram_size, 0>(data);
    bram1.write_array<uint32_t, bram_size, 0>(data);
    (void)bram0.read_array<uint32_t, bram_size>();
    (void)bram1.read_array<uint32_t, bram_size>();

    constexpr bool use_memcpy = true;
    constexpr int R = 1000;
    const double bytes_total = double(bram_size) * sizeof(uint32_t) * R;

    // AXI4-Lite WRITE
    fence();
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < R; ++i) {
        bram0.write_array<uint32_t, bram_size, 0>(data);
    }
    fence();
    auto t1 = std::chrono::steady_clock::now();
    double s = std::chrono::duration<double>(t1 - t0).count();
    logf<INFO>("AXI4LITE WRITE= {} s ({:.1f} MB/s)\n", s, bytes_total/s/1e6);

    // AXI4-Lite WRITE
    fence();
    t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < R; ++i) {
        bram0.write_array<uint32_t, bram_size , 0, use_memcpy>(data);
    }
    fence();
    t1 = std::chrono::steady_clock::now();
    s = std::chrono::duration<double>(t1 - t0).count();
    logf<INFO>("AXI4LITE WRITE (MEMCPY) = {} s ({:.1f} MB/s)\n", s, bytes_total/s/1e6);

    // AXI4 WRITE
    fence();
    t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < R; ++i) {
        bram1.write_array<uint32_t, bram_size, 0>(data);
    }
    fence();
    t1 = std::chrono::steady_clock::now();
    s = std::chrono::duration<double>(t1 - t0).count();
    logf<INFO>("AXI4 WRITE = {} s ({:.1f} MB/s)\n", s, bytes_total/s/1e6);

    // AXI4 MEMCPY WRITE
    fence();
    t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < R; ++i) {
        bram1.write_array<uint32_t, bram_size, 0, use_memcpy>(data);
    }
    fence();
    t1 = std::chrono::steady_clock::now();
    s = std::chrono::duration<double>(t1 - t0).count();
    logf<INFO>("AXI4 WRITE (MEMCPY) = {} s ({:.1f} MB/s)\n", s, bytes_total/s/1e6);

    // AXI4-Lite READ (use checksum so compiler can't delete it)
    fence();
    t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < R; ++i) {
        auto tmp = bram0.read_array<uint32_t, bram_size>();
        for (uint32_t v : tmp) {
            DO_NOT_OPTIMIZE(v);  // forces the load, no arithmetic, no stores
        }
    }
    fence();
    t1 = std::chrono::steady_clock::now();
    s = std::chrono::duration<double>(t1 - t0).count();
    logf<INFO>("AXI4LITE READ = {} s ({:.1f} MB/s)\n", s, bytes_total/s/1e6);

    // AXI4 READ
    fence();
    t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < R; ++i) {
        auto tmp = bram1.read_array<uint32_t, bram_size>();
        for (uint32_t v : tmp) {
            DO_NOT_OPTIMIZE(v);  // forces the load, no arithmetic, no stores
        }
    }
    fence();
    t1 = std::chrono::steady_clock::now();
    s = std::chrono::duration<double>(t1 - t0).count();
    logf<INFO>("AXI4 READ = {} s ({:.1f} MB/s)\n", s, bytes_total/s/1e6);

    return 0;
}