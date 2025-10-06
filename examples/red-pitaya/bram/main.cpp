#include "server/runtime/runtime.hpp"

#include <array>

int main() {
    koheron::Runtime rt;
    auto& ctx = rt.context();
    rt.systemd_notify_ready();

    auto& bram0 = ctx.mm.get<mem::bram0>();
    auto& bram1 = ctx.mm.get<mem::bram1>();
    constexpr uint32_t bram_size = mem::bram0_range/sizeof(uint32_t);

    std::array<uint32_t, bram_size> data;
    for (uint32_t i = 0; i < data.size(); ++i) data[i] = 0x12340000u + i;

    bram0.write_array<uint32_t, bram_size,0,false>(data);
    bram1.write_array<uint32_t, bram_size,0,false>(data);
    (void)bram0.read_array<uint32_t, bram_size>();
    (void)bram1.read_array<uint32_t, bram_size>();

    ctx.logf<INFO>("Bram size = {}", bram_size);

    return 0;
}