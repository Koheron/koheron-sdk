#include <cstdint>
#include <unistd.h>

#include "server/runtime/syslog.hpp"
#include "server/runtime/systemd.hpp"
#include "server/hardware/memory_manager.hpp"
#include "server/hardware/fpga_manager.hpp"
#include "server/hardware/zynq_fclk.hpp"
#include "server/drivers/uio.hpp"

int main() {
    hw::FpgaManager   fpga;
    hw::ZynqFclk      fclk;
    hw::MemoryManager mm;

    if (fpga.load_bitstream() < 0) {
        log<PANIC>("Failed to load bitstream.\n");
        return -1;
    }
    if (mm.open() < 0) {
        log<PANIC>("Failed to open memory\n");
        return -1;
    }

    rt::systemd::notify_ready();





    return 0;
}
