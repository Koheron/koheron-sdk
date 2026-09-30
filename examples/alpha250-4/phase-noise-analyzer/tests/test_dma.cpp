#include "../phase-dma.hpp"
#include <cassert>
#include <iostream>

int main() {
    std::atomic<bool> running{true};
    PhaseDma dma;
    // A stopped producer must not trap the caller waiting for initial data.
    assert(!dma.read_xy<65536>(0, running));
    dma.set_fs(scicpp::units::frequency<float>{5e6f});
    dma.start_acquisition();
    const auto check = [](const auto& block) {
        assert(block);
        const uint32_t start = (block->end_chunk - 8) * 8192;
        for (uint32_t i = 0; i < 65536; ++i) {
            assert(block->x[i] == int32_t(start + i));
            assert(block->y[i] == int32_t(start + i + 100));
        }
    };
    auto first = dma.read_xy<65536>(0, running);
    check(first);
    auto next = dma.read_xy<65536>(first->end_chunk, running);
    check(next);
    assert(next->end_chunk >= first->end_chunk + 8);
    // The window at chunk 2052 crosses the ring boundary at chunk 2048.
    auto wrap = dma.read_xy<65536>(2044, running);
    check(wrap);
    assert(wrap->end_chunk >= 2052);
    dma.configure_sampling(scicpp::units::frequency<float>{1e6f}, [&] {
        const auto completed = dma.completed_chunks();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        assert(dma.completed_chunks() == completed);
    });
    running.store(false);
    const auto before = std::chrono::steady_clock::now();
    assert(!dma.read_xy<65536>(UINT64_MAX, running));
    assert(std::chrono::steady_clock::now() - before < std::chrono::milliseconds(100));
    std::cout << "DMA freshness, X/Y alignment, ring wrap and cancellation tests passed\n";
}
