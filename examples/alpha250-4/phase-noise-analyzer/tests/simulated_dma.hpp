#pragma once
#include "../phase-dma.hpp"
#include <cassert>

// Autonomous descriptor engine: publication at TLAST precedes DDR writeback.
// It deliberately ignores stale Complete bits on subsequent cyclic laps.
class SimulatedDma {
    std::atomic<bool> running{true};
    std::thread worker;
  public:
    explicit SimulatedDma(std::chrono::microseconds packet_period = std::chrono::microseconds(100)) : worker([this, packet_period] {
        auto& ram=hw::get_memory<mem::ram>();
        auto& engine=hw::get_memory<mem::dma>();
        auto& ctl=hw::get_memory<mem::control>();
        auto& mux=hw::get_memory<mem::mux>();
        uint32_t epoch=UINT32_MAX, sequence=0, descriptor=0, sample=0;
        while(running.load()) {
            {
                std::lock_guard lock(hw::simulated_bus);
                if(ctl.read<reg::acquisition_run>() && (mux.read<0>()&0x10002u)==0x10002u &&
                   (engine.read<0x30>()&0x11u)==0x11u) {
                    if(epoch!=hw::simulated_epoch.load()) {
                        epoch=hw::simulated_epoch.load(); sequence=0;
                        descriptor=engine.read<0x38>();
                    }
                    assert(ram.read_reg(descriptor+24)==PhaseDma::chunk_bytes);
                    const auto address=ram.read_reg(descriptor+8);
                    const bool y=sequence%2;
                    for(uint32_t i=0;i<PhaseDma::samples_per_chunk;++i)
                        ram.data[address/4+i]=sample+i+(y?100:0);
                    const auto metadata=(y?hw::injected_y_status:hw::injected_x_status).load();
                    mux.registers[(0x1000+4*(sequence%1024))/4]=metadata;
                    hw::completed_packet_status.store((++sequence<<8)|metadata);
                    // DMA finishes descriptor writeback before the next packet.
                    ram.write_reg(descriptor+28,0x8C000000u|PhaseDma::chunk_bytes);
                    descriptor=ram.read_reg(descriptor);
                    if(y) sample+=PhaseDma::samples_per_chunk;
                }
            }
            std::this_thread::sleep_for(packet_period);
        }
    }) {}
    ~SimulatedDma() {running.store(false);worker.join();}
};
