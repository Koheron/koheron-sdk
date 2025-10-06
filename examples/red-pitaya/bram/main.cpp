#include "server/runtime/syslog.hpp"
#include "server/runtime/systemd.hpp"
#include "server/context/memory_manager.hpp"
#include "server/context/fpga_manager.hpp"
#include "server/context/zynq_fclk.hpp"

#include <array>

int main() {
    FpgaManager fpga;
    ZynqFclk fclk;
    MemoryManager mm;

    if (fpga.load_bitstream() < 0) {
        koheron::print<PANIC>("Failed to load bitstream.\n");
        return -1;
    }

    if (mm.open() < 0) {
        koheron::print<PANIC>("Failed to open memory");
        return -1;
    }

    zynq_clocks::set_clocks(fclk);
    systemd::notify_ready();

    auto& bram0 = mm.get<mem::bram0>();
    auto& bram1 = mm.get<mem::bram1>();
    constexpr uint32_t bram_size = mem::bram0_range/sizeof(uint32_t);

    std::array<uint32_t, bram_size> data;

    for (uint32_t i = 0; i < data.size(); ++i) {
        data[i] = 0x12340000u + i;
    }

    bram0.write_array<uint32_t, bram_size,0,false>(data);
    bram1.write_array<uint32_t, bram_size,0,false>(data);
    (void)bram0.read_array<uint32_t, bram_size>();
    (void)bram1.read_array<uint32_t, bram_size>();

    koheron::print_fmt<INFO>("Bram size = {}", bram_size);
    return 0;
}