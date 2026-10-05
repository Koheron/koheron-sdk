#include "../phase-dma.hpp"
#include <cassert>
#include <iostream>

// Autonomous descriptor engine: publication at TLAST precedes DDR writeback.
// It deliberately ignores stale Complete bits on subsequent cyclic laps.
class SimulatedDma {
    std::atomic<bool> running{true};
    std::thread worker;
  public:
    SimulatedDma() : worker([this] {
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
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }) {}
    ~SimulatedDma() {running.store(false);worker.join();}
};

int main() {
    std::atomic<bool> running{true};
    PhaseDma dma;
    assert(!dma.read_xy<65536>(0,running));
    SimulatedDma producer;
    dma.set_fs(scicpp::units::frequency<float>{5e6f});
    dma.start_acquisition();
    const auto check=[](const auto& block) {
        assert(block);
        for(uint32_t i=0;i<65536;++i) {
            assert(block->x[i]==block->x[0]+int32_t(i));
            assert(block->y[i]==block->x[i]+100);
        }
    };
    auto first=dma.read_xy<65536>(0,running);check(first);
    auto next=dma.read_xy<65536>(first->end_chunk,running);check(next);
    assert(next->end_chunk>=first->end_chunk+8);
    assert(next->x[0]>first->x.back());
    auto wrap=dma.read_xy<65536>(PhaseDma::ring_chunks-4,running);check(wrap);
    assert(wrap->end_chunk>=PhaseDma::ring_chunks+4);
    // Configuration pauses the producer, resets both sample histories, then
    // starts the descriptor ring on X at its next absolute chunk slot.
    const auto before=dma.completed_chunks();
    std::optional<PhaseDma::Snapshot<65536>> during_configuration;
    std::thread reader([&] {
        during_configuration=dma.read_xy<65536>(before+80,running);
    });
    dma.configure_sampling(scicpp::units::frequency<float>{1e6f},[&] {
        const auto stopped=dma.completed_chunks();
        assert(stopped>=before);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        assert(dma.completed_chunks()==stopped);
    });
    reader.join();check(during_configuration);
    uint64_t consumed=dma.completed_chunks()+8;
    hw::injected_x_status.store(8);hw::injected_y_status.store(8);
    auto precise=dma.read_xy<65536>(consumed,running);check(precise);
    assert(precise->matches_precision(8)&&!precise->overflow&&!precise->sample_gap);
    consumed=dma.completed_chunks()+8;
    hw::injected_y_status.store(7);
    assert(!dma.read_xy<65536>(consumed,running)->matches_precision(8));
    consumed=dma.completed_chunks()+8;
    hw::injected_y_status.store(8|0x10u);
    auto clipped=dma.read_xy<65536>(consumed,running);
    assert(clipped->matches_precision(8)&&clipped->overflow);
    consumed=dma.completed_chunks()+8;
    hw::injected_x_status.store(8|0x40u);hw::injected_y_status.store(8);
    auto gap=dma.read_xy<65536>(consumed,running);check(gap);
    assert(gap->matches_precision(8)&&gap->sample_gap&&!gap->overflow);
    // The ring continues without any software rearming, even while no client
    // is consuming data. A fresh window after a long delay stays contiguous.
    const auto paused=dma.completed_chunks();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    auto resumed=dma.read_xy<65536>(paused,running);check(resumed);
    assert(resumed->end_chunk>paused+8);
    hw::get_memory<mem::dma>().registers[0x34/4]=0x10;
    assert(!dma.read_xy<65536>(UINT64_MAX,running));
    running.store(false);
    assert(!dma.read_xy<65536>(UINT64_MAX,running));
    std::cout<<"Continuous DMA ring: freshness, pairing, wrap, epoch restart, precision/gap metadata and cancellation passed\n";
}
