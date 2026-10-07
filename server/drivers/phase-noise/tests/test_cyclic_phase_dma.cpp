#include "server/drivers/phase-noise/cyclic-phase-dma.hpp"
#include <cassert>
#include <iostream>
#include <memory>

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
                if(!hw::producer_paused.load() && ctl.read<reg::acquisition_run>() && (mux.read<0>()&0x10002u)==0x10002u &&
                   (engine.read<0x30>()&0x11u)==0x11u) {
                    if(epoch!=hw::simulated_epoch.load()) {
                        epoch=hw::simulated_epoch.load(); sequence=0;
                        descriptor=engine.read<0x38>();
                    }
                    assert(mux.read<0>() & (1u<<17));
                    assert(ram.read_reg(descriptor+24)==CyclicPhaseDma::chunk_bytes);
                    const auto address=ram.read_reg(descriptor+8);
                    const auto metadata=hw::injected_x_status.load();
                    mux.registers[(0x1000+4*(sequence%1024))/4]=metadata;
                    hw::completed_packet_status.store((++sequence<<8)|metadata);
                    // TLAST arrives while the current DDR payload and stale
                    // cyclic Complete bit still belong to the previous lap.
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                    for(uint32_t i=0;i<CyclicPhaseDma::samples_per_chunk;++i)
                        ram.data[address/4+i]=sample+i;
                    ram.write_reg(descriptor+28,
                        (hw::inject_descriptor_error.load() ? 0x9C000000u : 0x8C000000u) | CyclicPhaseDma::chunk_bytes);
                    descriptor=ram.read_reg(descriptor);
                    sample+=CyclicPhaseDma::samples_per_chunk;
                }
            }
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }) {}
    ~SimulatedDma() {running.store(false);worker.join();}
};

template<class Block> void check(const Block& block) {
    assert(block);
    for(uint32_t i=0;i<block->samples.size();++i) {
        assert(block->samples[i]==block->samples[0]+int32_t(i));
    }
}

void check_overrun_recovery(CyclicPhaseDma& dma, const std::atomic<bool>& running, uint64_t old_end) {
    const auto old_generation=dma.generation();
    auto recovered=dma.read<32768>(old_end,running,2,true);check(recovered);
    assert(recovered->skipped_hops>0);
    assert(recovered->end_chunk==old_end+2*(1+recovered->skipped_hops));
    assert(recovered->generation==old_generation && dma.generation()==old_generation);
    assert(dma.overruns()==1);
    auto after_recovery=dma.read<32768>(recovered->end_chunk,running,2,true);check(after_recovery);
    assert(after_recovery->end_chunk==recovered->end_chunk+2 && after_recovery->skipped_hops==0);
}

int main() {
    std::atomic<bool> running{true};
    CyclicPhaseDma dma;
    assert(!dma.read<65536>(0,running));
    SimulatedDma producer;
    dma.configure_sampling(scicpp::units::frequency<float>{5e6f}, []{});
    dma.start_acquisition();

    auto first=dma.read<65536>(0,running);check(first);
    auto next=dma.read<65536>(first->end_chunk,running);check(next);
    assert(next->end_chunk>=first->end_chunk+8);
    assert(next->samples[0]>first->samples.back());
    auto shorter=dma.read<32768>(next->end_chunk,running);check(shorter);
    assert(shorter->end_chunk>=next->end_chunk+4);
    assert(shorter->samples[0]>next->samples.back());
    auto stream_first=dma.read<32768>(shorter->end_chunk,running,2,false);check(stream_first);
    auto stream_next=dma.read<32768>(stream_first->end_chunk,running,2,true);check(stream_next);
    assert(stream_next->end_chunk==stream_first->end_chunk+2);
    assert(stream_next->samples[0]==stream_first->samples[16384]);
    assert(stream_next->generation==stream_first->generation);
    // Reuse dirty scratch storage without retaining metadata from a previous
    // failed capture, and keep the published phase window untouched.
    auto reused = std::make_unique<CyclicPhaseDma::Snapshot<32768>>();
    reused->overflow = reused->sample_gap = reused->mixed_precision = true;
    reused->skipped_hops = 123;
    const auto published = std::make_unique<std::array<int32_t, 32768>>(stream_next->samples);
    const auto* storage = reused->samples.data();
    assert(dma.read_into(*reused, stream_next->end_chunk, running, 2, true));
    check(reused);
    assert(reused->samples.data() == storage && stream_next->samples == *published);
    assert(!reused->overflow && !reused->sample_gap && !reused->mixed_precision && !reused->skipped_hops);
    assert(reused->samples[0] == (*published)[16384]);
    auto wrap=dma.read<65536>(CyclicPhaseDma::ring_chunks-4,running);check(wrap);
    assert(wrap->end_chunk>=CyclicPhaseDma::ring_chunks+4);
    // Configuration pauses the producer, resets phase and filter histories, then
    // starts the descriptor ring at its next absolute chunk slot.
    const auto before=dma.completed_chunks();
    std::optional<CyclicPhaseDma::Snapshot<65536>> during_configuration;
    std::thread reader([&] {
        during_configuration=dma.read<65536>(before+80,running);
    });
    dma.configure_sampling(scicpp::units::frequency<float>{1e6f},[&] {
        const auto stopped=dma.completed_chunks();
        assert(stopped>=before);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        assert(dma.completed_chunks()==stopped);
    });
    reader.join();check(during_configuration);
    uint64_t consumed=dma.completed_chunks()+8;
    hw::injected_x_status.store(8);
    auto precise=dma.read<65536>(consumed,running);check(precise);
    assert(precise->matches_precision(8)&&!precise->overflow&&!precise->sample_gap);
    consumed=dma.completed_chunks()+8;
    hw::injected_x_status.store(7);
    assert(!dma.read<65536>(consumed,running)->matches_precision(8));
    consumed=dma.completed_chunks()+8;
    hw::injected_x_status.store(8|0x10u);
    auto clipped=dma.read<65536>(consumed,running);
    assert(clipped->matches_precision(8)&&clipped->overflow);
    consumed=dma.completed_chunks()+8;
    hw::injected_x_status.store(8|0x40u);
    auto gap=dma.read<65536>(consumed,running);check(gap);
    assert(gap->matches_precision(8)&&gap->sample_gap&&!gap->overflow);
    // The ring continues without any software rearming, even while no client
    // is consuming data. A fresh window after a long delay stays contiguous.
    const auto paused=dma.completed_chunks();
    hw::injected_x_status.store(8);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    auto resumed=dma.read<65536>(paused,running);check(resumed);
    assert(resumed->end_chunk>paused+8);
    assert(resumed->generation==dma.generation());
    // Reset on a non-zero slot must align descriptor and metadata indices.
    for(unsigned i=0;i<3;++i) {
        const auto prior_generation=dma.generation();
        const auto prior_count=dma.completed_chunks();
        dma.configure_sampling(scicpp::units::frequency<float>{1e6f},[]{});
        auto fresh=dma.read<65536>(prior_count,running);check(fresh);
        assert(fresh->generation==prior_generation+1);
        assert(fresh->end_chunk>=prior_count+9); // first packet is filter settling
    }
    hw::injected_x_status.store(8|0x20u);
    auto mixed=dma.read<65536>(dma.completed_chunks()+8,running);check(mixed);
    assert(mixed->mixed_precision&&!mixed->matches_precision(8));
    hw::inject_descriptor_error.store(true);
    assert(!dma.read<65536>(dma.completed_chunks()+16,running));
    hw::inject_descriptor_error.store(false);
    dma.configure_sampling(scicpp::units::frequency<float>{5e6f},[]{});
    dma.start_acquisition();
    check(dma.read<65536>(dma.completed_chunks(),running));
    // A stopped producer returns an error promptly, even if there is no DMA
    // error bit. This watchdog also bounds destructor cancellation.
    hw::producer_paused.store(true);
    auto stalled=std::chrono::steady_clock::now();
    assert(!dma.read<65536>(UINT64_MAX,running));
    assert(std::chrono::steady_clock::now()-stalled<std::chrono::seconds(1));
    hw::producer_paused.store(false);
    dma.configure_sampling(scicpp::units::frequency<float>{5e6f},[]{});
    dma.start_acquisition();
    check(dma.read<65536>(dma.completed_chunks(),running));
    // An overwritten hop resumes with a fully contiguous recent window,
    // explicit coverage loss, and no producer reset or generation change.
    const auto old_end=dma.completed_chunks();
    const auto overrun_deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    while(dma.completed_chunks()<old_end+CyclicPhaseDma::ring_chunks+4) {
        assert(std::chrono::steady_clock::now()<overrun_deadline);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    check_overrun_recovery(dma, running, old_end);
    dma.configure_sampling(scicpp::units::frequency<float>{5e6f},[]{});
    check(dma.read<32768>(dma.completed_chunks(),running));
    hw::get_memory<mem::dma>().registers[0x34/4]=0x10;
    assert(!dma.read<65536>(UINT64_MAX,running));
    running.store(false);
    assert(!dma.read<65536>(UINT64_MAX,running));
    std::cout<<"Continuous DMA ring: freshness, wrap, epoch restart, slow consumers, precision/overflow/gap metadata and cancellation passed\n";
}
