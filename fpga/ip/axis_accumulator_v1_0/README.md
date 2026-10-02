# AXI4-Stream float accumulator

`koheron:user:axis_accumulator:1.0` sums corresponding float32 bins across
`N_FRAMES` input frames and emits one frame of sums. It packages the control,
two accumulation BRAM banks and a Xilinx Floating-Point 7.1 adder behind
standard slave/master AXI4-Stream interfaces. Normalization remains in software.

## Interface

All ports use `aclk`. `aresetn` is a synchronous active-low reset; assert it for
at least two clock edges. Reset discards partial groups and pending outputs,
clears status and mutes `TVALID`. BRAM does not need initialization: the first
frame overwrites every bin using a zero accumulator operand.

| Port | Meaning |
| --- | --- |
| `S_AXIS.TDATA[31:0]` | IEEE-754 single-precision sample |
| `S_AXIS.TVALID/TREADY` | Input accepted only when both are high |
| `S_AXIS.TLAST` | Last bin of each input frame |
| `M_AXIS.TDATA[31:0]` | Sum of the corresponding bins, in input order |
| `M_AXIS.TVALID/TREADY` | Standard output handshake; all fields remain stable while stalled |
| `M_AXIS.TLAST` | Last bin of each output sum |
| `M_AXIS.TUSER[31:0]` | Zero-based bin index |
| `frame_index[31:0]` | Current input frame within the accumulation group, starting at zero |
| `frame_error` | Sticky framing error, cleared by reset |
| `result_count[31:0]` | Completed output frames; increments on the last output handshake and wraps modulo 2^32 |
| `cycle_index[31:0]` | Compatibility progress for existing real-time BRAM readers (see below) |

`FRAME_LENGTH` and `N_FRAMES` are positive build-time parameters, each bounded
to 65536 by the package GUI. The arithmetic is fixed float32, one pipelined add
per bin per frame, with an eight-clock vendor-adder latency. Its rounding and
special-value behavior are those of the Xilinx Floating-Point core; this is
not a compensated summation or a higher-precision accumulator.

`SYNC_ON_RESET` defaults to 0: the first accepted sample is bin zero of a new
frame. Set it to 1 with `CHECK_TLAST=1` when the upstream pipeline can continue
running through a reset of this IP. In that mode, input is discarded through
the first accepted TLAST without setting `frame_error`. The next sample starts
bin zero. A complete first frame is also discarded if reset happens exactly at
a frame boundary. This makes reset alignment independent of upstream latency.

`CHECK_TLAST` defaults to 1. An early or late TLAST discards the entire current
partial accumulation group and sets `frame_error`. For a missing/late TLAST,
input is consumed and discarded through the next accepted TLAST. The next
frame starts a fresh group. A malformed group never emits a partial result.
Previously completed groups continue draining normally. A stream that never
sends TLAST after a framing error requires reset to recover.

Set `CHECK_TLAST=0` only for legacy fixed-length streams without frame markers.
In that mode TLAST is ignored and every `FRAME_LENGTH` accepted samples forms a
frame. No framing errors can be detected in this mode.

## Buffering and throughput

Each bank belongs to an accumulation group from its first input handshake until
its final beat is copied into the elastic output register. That register holds
the beat until the downstream handshake, allowing safe reuse of the bank while
still preserving backpressure. One bank can drain while the other accumulates;
completed frames are emitted in order. Publication waits until the last sum
has reached BRAM, so backpressure cannot expose unfinished bins. When both
banks are owned, the IP deasserts input TREADY until a bank becomes free.
The upstream producer must then retain its beat or buffer it externally.
Completion status advances only on the final downstream handshake, so copying
the last beat into the output register cannot signal a premature completion.

A read-after-write interlock stalls repeated accesses to a bin while its
previous add is in flight. This supports even one-bin frames correctly.
Frames of at least eleven bins do not stall for this dependency at continuous
input. Output starts after the final group write and then runs at one bin per
clock when the receiver stays ready; switching output banks takes a clock.
Finite buffering cannot guarantee uninterrupted input under arbitrary output
backpressure or indefinitely sustain single-frame groups at one input per clock.

## Existing PSD examples

`fpga/lib/axis_accumulator.tcl` adapts the IP to the existing real-time PSD
streams and byte-addressed BRAM recorders. The PSD pipelines now preserve the
FFT's TLAST through their vendor floating-point multipliers and adder. The
adapter selects `CHECK_TLAST=1` and `SYNC_ON_RESET=1`, holds output TREADY high,
converts TUSER to a byte address and TVALID to four write
strobes. It requires at least 16 bins and three frames per sum: with that
contract, banks drain before reuse and the producer need not honor TREADY.
The four migrated examples retain their memory maps and software normalization.

The old counter wrapped before the adder pipeline had drained. `cycle_index`
now holds at `N_FRAMES-1` while a completed input group awaits output, and then
returns to input progress once its last result has been written to the recorder.
Existing software can continue detecting progress wrap, with complete results
already in BRAM. This compatibility indicator is for the adapter's dimensions
and always-ready consumer. Native stream consumers should use the output
handshake/TLAST and `result_count`; `cycle_index` is not a general completion
counter for one/two-frame groups or prolonged backpressure. BRAM readout still
requires the reader to finish before a later result overwrites the recorder.

The ALPHA250, ALPHA250-4, ALPHA15 and Red Pitaya FFT paths instantiate this
adapter. The former Tcl accumulator and PSD counter remain available for other
projects and their existing regressions.

## Build and verification

From the SDK root, after sourcing Vivado's `settings64.sh`:

```sh
vivado -mode batch -source fpga/vivado/core.tcl \
  -tclargs fpga/ip/axis_accumulator_v1_0 xc7z020clg400-2 tmp/axis-accumulator/cores
vivado -mode batch -source fpga/ip/axis_accumulator_v1_0/tests/run_sim.tcl \
  -tclargs tmp/axis-accumulator/sim
vivado -mode batch -source fpga/ip/axis_accumulator_v1_0/tests/package_smoke.tcl \
  -tclargs tmp/axis-accumulator/cores tmp/axis-accumulator/catalog
vivado -mode batch -source fpga/ip/axis_accumulator_v1_0/tests/run_psd_frames.tcl \
  -tclargs tmp/axis-accumulator/psd-frames
vivado -mode batch -source fpga/ip/axis_accumulator_v1_0/tests/synth.tcl \
  -tclargs tmp/axis-accumulator/cores tmp/axis-accumulator/route
```

The stream simulations use the actual vendor floating-point adder, checking
signed/fractional sums, bin indices, TLAST, pauses, output backpressure, bank
turnover, early/late TLAST recovery, reset and one-bin/short frames. The catalog
test builds the actual adapter and verifies continuous real-time input, output
addresses and complete BRAM writes before progress wrap, then synthesizes it
without unresolved black boxes. The route script checks internal setup/hold at
250 MHz for 2048- and 8192-bin packages; it is an out-of-context check without
board I/O constraints. Board-level routing and analog measurements are separate.

### Validation results

Vivado 2026.1 on `xc7z020clg400-2`, `N_FRAMES=1023`, native TLAST validation:

| Bins | LUTs | Flip-flops | BRAM36 | DSPs | Setup slack at 250 MHz | Hold slack |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 2048 | 563 | 948 | 4 | 0 | +0.222 ns | +0.098 ns |
| 8192 | 572 | 963 | 16 | 0 | +0.076 ns | +0.083 ns |

These counts include the embedded floating-point adder and both banks, and
exclude the example's result recorder. Two banks consume twice the accumulation
storage of the former single-bank module. The 8192-bin setup margin is narrow;
integration and changed configurations require their own timing checks.

All twelve stream simulation profiles passed, including production-size 8192-bin
frames, the ten/eleven-bin pipeline boundary, reset during a partial group and
while a completed output is stalled. The packaged adapter simulation and
synthesis passed without vendor black boxes. The vendor PSD arithmetic
regressions cover TLAST/data alignment through pauses in both blocking and
nonblocking pipelines. All four migrated instrument configurations validated,
generated their full block designs and built bitstreams with `ENFORCE_TIMING=1`:

| Instrument | Setup slack | Hold slack | Bus-skew constraints checked |
| --- | ---: | ---: | ---: |
| ALPHA250 FFT | +0.179 ns | +0.004 ns | 6 |
| ALPHA250-4 FFT | +0.129 ns | +0.018 ns | 6 |
| ALPHA15 signal analyzer | +0.366 ns | +0.027 ns | 16 |
| Red Pitaya FFT | 0.000 ns | +0.007 ns | 11 |

These are whole-design margins at the examples' existing clocks. Red Pitaya
uses `Performance_NetDelay_high`; it and both ALPHA250 examples enable
post-route physical optimization with `ExploreWithAggressiveHoldFix`. No
clock periods or timing exceptions were relaxed. Red Pitaya has no setup
margin at the reported precision, so changed placement or configurations need
another strict timing check. The SDK reports existing incomplete external
I/O delay constraints; the passing checks cover constrained paths, pulse width
and bus skew. Live board operation and analog measurements remain unverified.

To reproduce the full builds with Vivado 2026.1:

```sh
make fpga CFG=examples/alpha250/fft/config.mk VIVADO_VERSION=2026.1 ENFORCE_TIMING=1 N_CPUS=4
make fpga CFG=examples/alpha250-4/fft/config.mk VIVADO_VERSION=2026.1 ENFORCE_TIMING=1 N_CPUS=4
make fpga CFG=examples/alpha15/signal-analyzer/config.mk VIVADO_VERSION=2026.1 ENFORCE_TIMING=1 N_CPUS=4
make fpga CFG=examples/red-pitaya/fft/config.mk VIVADO_VERSION=2026.1 ENFORCE_TIMING=1 N_CPUS=4
```
