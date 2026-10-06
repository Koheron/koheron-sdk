#include "../../../../server/drivers/dma-s2mm.hpp"
#include <cassert>
#include <iostream>
int main() {
    auto& registers=hw::get_memory<mem::dma>();
    DmaS2MM dma;
    registers.words[0x34/4]=10; // SGIncld + Idle
    dma.start_transfer(0x1e000000,4096);
    assert(registers.writes==0);
    assert(!dma.wait_for_transfer_checked(.001f));
    registers.words[0x34/4]=2; // existing simple-mode instruments
    dma.start_transfer(0x1e000000,4096);
    assert(registers.words[0x48/4]==0x1e000000);
    assert(registers.words[0x58/4]==4096);
    assert(dma.wait_for_transfer_checked(.001f));
    std::cout << "PASS: legacy simple DMA cannot reset the SG ring; simple mode remains operational\n";
}
