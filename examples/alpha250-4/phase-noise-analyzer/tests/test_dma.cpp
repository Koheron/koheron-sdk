#include "../phase-dma.hpp"
#include <cassert>
#include <iostream>
#include <memory>

#include "simulated_dma.hpp"

template<class Block> void check(const Block& block) {
    assert(block);
    for(uint32_t i=0;i<block->x.size();++i) {
        assert(block->x[i]==block->x[0]+int32_t(i));
        assert(block->y[i]==block->x[i]+100);
    }
}

void check_overrun_recovery(PhaseDma& dma, const std::atomic<bool>& running, uint64_t old_end) {
    const auto old_generation=dma.generation();
    auto recovered=dma.read_xy<32768>(old_end,running,2,true);check(recovered);
    assert(recovered->skipped_hops>0);
    assert(recovered->end_chunk==old_end+2*(1+recovered->skipped_hops));
    assert(recovered->generation==old_generation && dma.generation()==old_generation);
    assert(dma.overruns()==1);
    auto after_recovery=dma.read_xy<32768>(recovered->end_chunk,running,2,true);check(after_recovery);
    assert(after_recovery->end_chunk==recovered->end_chunk+2 && after_recovery->skipped_hops==0);
}

int main() {
    std::atomic<bool> running{true};
    PhaseDma dma;
    assert(!dma.read_xy<65536>(0,running));
    SimulatedDma producer;
    dma.set_fs(scicpp::units::frequency<float>{5e6f});
    dma.start_acquisition();

    auto first=dma.read_xy<65536>(0,running);check(first);
    auto next=dma.read_xy<65536>(first->end_chunk,running);check(next);
    assert(next->end_chunk>=first->end_chunk+8);
    assert(next->x[0]>first->x.back());
    auto shorter=dma.read_xy<32768>(next->end_chunk,running);check(shorter);
    assert(shorter->end_chunk>=next->end_chunk+4);
    assert(shorter->x[0]>next->x.back());
    auto stream_first=dma.read_xy<32768>(shorter->end_chunk,running,2,false);check(stream_first);
    auto stream_next=dma.read_xy<32768>(stream_first->end_chunk,running,2,true);check(stream_next);
    assert(stream_next->end_chunk==stream_first->end_chunk+2);
    assert(stream_next->x[0]==stream_first->x[16384]);
    assert(stream_next->generation==stream_first->generation);
    auto reused = std::make_unique<PhaseDma::Snapshot<32768>>();
    reused->overflow = reused->sample_gap = reused->mixed_precision = true;
    reused->skipped_hops = 123;
    const auto published = std::make_unique<PhaseDma::Snapshot<32768>>(*stream_next);
    const auto* storage = reused->x.data();
    assert(dma.read_into(*reused, stream_next->end_chunk, running, 2, true));
    check(reused);
    assert(reused->x.data() == storage && stream_next->x == published->x && stream_next->y == published->y);
    assert(!reused->overflow && !reused->sample_gap && !reused->mixed_precision && !reused->skipped_hops);
    assert(reused->x[0] == published->x[16384] && reused->y[0] == published->y[16384]);
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
    // An overwritten hop resumes with a fully contiguous recent window,
    // explicit coverage loss, and no producer reset or generation change.
    const auto old_end=dma.completed_chunks();
    const auto overrun_deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    while(dma.completed_chunks()<old_end+PhaseDma::ring_chunks+4) {
        assert(std::chrono::steady_clock::now()<overrun_deadline);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    check_overrun_recovery(dma, running, old_end);
    dma.configure_sampling(scicpp::units::frequency<float>{5e6f},[]{});
    check(dma.read_xy<32768>(dma.completed_chunks(),running));
    hw::get_memory<mem::dma>().registers[0x34/4]=0x10;
    assert(!dma.read_xy<65536>(UINT64_MAX,running));
    running.store(false);
    assert(!dma.read_xy<65536>(UINT64_MAX,running));
    std::cout<<"Continuous DMA ring: freshness, pairing, wrap, epoch restart, precision/gap metadata and cancellation passed\n";
}
