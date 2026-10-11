# Vector network analyzer acquisition

Add `vector-network-analyzer.hpp` to `DRIVERS`. The board project supplies
`vna-clock.hpp`, defining `VNAClock::rate`, `mutex()`, `ensure()` and `matches()`.
Only the RPC wrapper is registered; policies and capture helpers are internal.
ALPHA250 uses its existing sampling-clock mutex; Red Pitaya has a fixed clock.

`sweep-plan.hpp` computes the integer logarithmic-chirp coefficient, 16 seeds,
sample count and Q15 gain. `capture.hpp` owns the fixed DMA region, 512 scatter/
gather descriptors, capture state and renewable client lease. RPC tokens
prevent overlapping clients from reseeding or mixing records. A 30-second
lease recovers abandoned captures; capture completion has a five-second timeout.

Status checks validate the clock, FIFO overflow, near-rail ADC samples, DMA
faults and every descriptor length before permitting downloads. Returned blocks
own their copies so concurrent cancellation cannot corrupt serialization.
Enabling high OCM for descriptors preserves the boot mapping of all other banks;
replacing that mapping can alias OCM over Linux DDR and hang the board.

The FPGA assembly and both boards use matching `memory.yml` register/DMA maps.
DMA RAM at `0x18000000` must have a boot-time `no-map` reservation. See the
[shared workspace](../../../web/vector-network-analyzer/README.md) for protocol,
processing and tests; board examples report hardware checks separately.

## RPC contract

`begin_sweep(start, stop, duration, level)` uses the full 64 Mi-sample record.
`begin_recorded_sweep(start, stop, duration, level, samples)` selects a power-of-two
record of 4–64 Mi samples containing the chirp. Both use excitation endpoints
in Hz, duration in seconds and digital level in dBFS, and return `(token, error)`:
0 success, 1 invalid settings, 2 busy, 3 DMA reset timeout. Token zero means
nothing started. The browser expands displayed endpoints before this call.

`get_plan(token)` returns 36 unsigned 32-bit words: sample count, Q15 gain,
coefficient low/high words, then low/high words for each of the 16 seeds.
`get_status(token)` returns 0 running, 1 complete, 2 overflow, 3 clipping,
4 DMA error, 5 stale/expired token, 6 capture timeout or 7 clock changed.
`get_adc_block(token, index)` returns one 256 KiB block of packed signed 16-bit
samples only after successful completion. `release(token)` cancels or releases
that client's capture and returns false for an old token.

`get_record_samples(token)` returns the selected record length, or zero for an
invalid token. `get_adc_blocks(token, index, count)` returns 1–16 consecutive
256 KiB blocks in one RPC, bounded by that record. Invalid requests return an
empty vector. The single-block method uses the same checked path.
