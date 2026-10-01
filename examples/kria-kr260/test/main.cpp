#include "server/runtime/syslog.hpp"
#include "server/runtime/systemd.hpp"
#include "server/hardware/memory_manager.hpp"
#include "server/hardware/fpga_manager.hpp"
#include "server/hardware/zynq_fclk.hpp"

#include <chrono>
#include <thread>
#include <cstdint>

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

    rt::systemd::notify_ready();

    while (true) {
        using namespace std::chrono_literals;
        log("Server alive\n");
        std::this_thread::sleep_for(1s);
    }

    return 0;
}
