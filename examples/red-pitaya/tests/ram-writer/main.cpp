#include "server/runtime/syslog.hpp"
#include "server/runtime/systemd.hpp"
#include "server/hardware/memory_manager.hpp"
#include "server/hardware/fpga_manager.hpp"
#include "server/hardware/zynq_fclk.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <memory>

using namespace std::chrono;
using namespace std::chrono_literals;

#define CMA_ALLOC _IOWR('Z', 0, uint32_t)

int main() {
    hw::FpgaManager fpga; hw::ZynqFclk fclk;
    if (fpga.load_bitstream() < 0) { log<PANIC>("Failed to load bitstream.\n"); return -1; }

    auto mm = services::provide<hw::MemoryManager>();
    if (mm->open() < 0)            { log<PANIC>("Failed to open memory\n");      return -1; }

    // 100 MHz fabric clock
    fclk.set("fclk0", 100000000);
    rt::systemd::notify_ready();

    int fd;

    if((fd = open("/dev/cma", O_RDWR)) < 0)
    {
        perror("open");
        return EXIT_FAILURE;
    }

    uint32_t size = 2048*sysconf(_SC_PAGESIZE);
    logf("size = {}\n", size);

    if(ioctl(fd, CMA_ALLOC, &size) < 0)
    {
        perror("ioctl");
        return EXIT_FAILURE;
    }

    volatile uint32_t *ram = reinterpret_cast<uint32_t*>(mmap(NULL, 2048*sysconf(_SC_PAGESIZE), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0));

    if (ram == nullptr) {
        log("ram == nullptr");
    }

    auto& ctl = hw::get_memory<mem::control>();

    ctl.write<reg::min_addr>(size);
    ctl.write<reg::cfg_data>(0xFFFF);

    auto& sts = hw::get_memory<mem::status>();

    while(true) {
        uint32_t position = sts.read<reg::sts_data>();
        logf("position = {}\n", position);

        logf("ram[0] = {}\n", ram[0]);
        logf("ram[-1] = {}\n", ram[100000]);
        std::this_thread::sleep_for(100ms);
    }

    return 0;
}
