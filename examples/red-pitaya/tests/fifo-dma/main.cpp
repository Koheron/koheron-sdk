#include <cstdint>
#include <unistd.h>

#include "server/runtime/syslog.hpp"
#include "server/runtime/systemd.hpp"
#include "server/context/memory_manager.hpp"
#include "server/context/fpga_manager.hpp"
#include "server/context/zynq_fclk.hpp"
#include "server/drivers/uio.hpp"

int main() {
    FpgaManager   fpga;
    ZynqFclk      fclk;
    MemoryManager mm;

    if (fpga.load_bitstream() < 0) {
        koheron::print<PANIC>("Failed to load bitstream.\n");
        return -1;
    }
    if (mm.open() < 0) {
        koheron::print<PANIC>("Failed to open memory\n");
        return -1;
    }

    systemd::notify_ready();





    return 0;
}
