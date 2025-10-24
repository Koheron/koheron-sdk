#include "server/runtime/syslog.hpp"
#include "server/runtime/systemd.hpp"
#include "server/hardware/memory_manager.hpp"
#include "server/hardware/fpga_manager.hpp"
#include "server/hardware/zynq_fclk.hpp"
#include "server/hardware/remoteproc_manager.hpp"

#include <chrono>
#include <thread>
#include <cstdint>
#include <csignal>

namespace {
volatile std::sig_atomic_t g_run = 1;

void handle_signal(int) {
    g_run = 0;
}
} // namespace

int main() {
    hw::FpgaManager fpga;
    hw::ZynqFclk fclk;
    hw::MemoryManager mm;
    hw::RpuFirmwareManager rpu;

    if (fpga.load_bitstream() < 0) {
        log<PANIC>("Failed to load bitstream.\n");
        return -1;
    }
    if (mm.open() < 0) {
        log<PANIC>("Failed to open memory");
        return -1;
    }

    if (rpu.install_all() < 0) {
        log<ERROR>("Failed to install Cortex-R5 firmware\n");
    }
    if (rpu.start_all() < 0) {
        log<ERROR>("Failed to start Cortex-R5 firmware\n");
    }
    for (const auto& remoteproc : rpu.known_remoteprocs()) {
        log("Managing remote processor %s\n", remoteproc.c_str());
    }

    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    rt::systemd::notify_ready();

    while (g_run) {
        using namespace std::chrono_literals;
        log("Server alive\n");
        std::this_thread::sleep_for(1s);
    }

    if (rpu.stop_all() < 0) {
        log<ERROR>("Failed to stop Cortex-R5 firmware\n");
    }

    return 0;
}
