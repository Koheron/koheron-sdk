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

    fclk.set("fclk0", 200000000);
    fclk.set("fclk1", 50000000);
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

    volatile uint64_t *ram = reinterpret_cast<uint64_t*>(mmap(NULL, 2048*sysconf(_SC_PAGESIZE), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0));

    if (ram == nullptr) {
        log("ram == nullptr");
    }

    auto& ctl = hw::get_memory<mem::control>();

    ctl.write<reg::min_addr>(size);
    ctl.write<reg::cfg_data>(0xFFFF);

    uint32_t position_prev = 0;
    uint32_t position = 0;
    uint64_t count, count_prev = 0;

    auto& sts = hw::get_memory<mem::status>();

    while (true) {
        position = sts.read<reg::sts_data>();

        if (position_prev < 32*1024 && position > 32*1024) {
            count = ram[512*1024-1];
            logf("Region LOW ready, position = {}, count = {}, delta = {}\n", position, count, count-count_prev);
        } else if (position_prev > position) {
            count = ram[1024 * 1024 - 1];
            logf("Region HIGH ready, position = {}, count = {}, delta = {}\n", position, count, count-count_prev);
        } else {
            std::this_thread::sleep_for(1ms);
        }
        position_prev = position;
        count_prev = count;
    }

    return 0;
}
