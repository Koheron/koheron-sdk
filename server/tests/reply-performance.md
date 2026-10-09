# Shared reply-path performance

Validated on 2026-10-08 against V1 `bf41e191`. These changes are in `net::Session`
and `ut::RateTracker`, shared by network-enabled examples, including TCP,
WebSocket and Unix socket clients. They do not depend on an instrument's DSP or
FPGA design.

## Changes

Small scalar replies and nested scalar tuples, up to 512 bytes including the
header, are packed directly into a bounded stack buffer. This avoids clearing,
growing and updating a dynamic serialization buffer for every field. Wire
endianness, tuple field order and synchronous transport behavior are preserved.
Strings, containers and larger tuples keep their existing paths.

Array, vector and span replies already record their send-completion time.
Duration-based rate accounting now accepts that timestamp, eliminating a
redundant `steady_clock::now()` call. Existing two-argument calls continue to
work; byte totals and distribution over duration buckets are retained.

## CPU measurements on Red Pitaya

`benchmark_replies.cpp` exercises the production `Session::send` and rate
accounting with opaque synchronous send hooks. It measures packing/accounting
CPU time without socket I/O, framing, payload copies or driver work. The hooks
make the header and payload observable to the compiler without scanning them.
The bulk cases use 4096-byte arrays/vectors; status cases use eight scalar
fields in a flat or nested tuple.

Both binaries use GCC 13, `-O3 -flto -fno-math-errno`, ARMv7 hard float and NEON,
matching the Red Pitaya PNA server flags. Each case reports the median of fifteen
100000-call batches. Baseline/candidate/candidate/baseline runs were pinned to
CPU 1 on the Red Pitaya, with the original instrument still running. The table
averages the two medians per version.

| Reply shape | Baseline µs | Candidate µs | Reduction |
| --- | ---: | ---: | ---: |
| scalar | 1.913 | 1.872 | 2.1% |
| status | 2.227 | 1.930 | 13.4% |
| nested | 2.270 | 1.929 | 15.0% |
| array | 4.771 | 3.508 | 26.5% |
| vector | 4.746 | 3.566 | 24.9% |

These are reductions in shared reply overhead, not in complete instrument
processing or network round-trip time. Bulk transmission, acquisition and FFT
costs are excluded. The measured gains apply to these reply shapes and this
Cortex-A9 host; other processors need their own measurements.

## Build and regression checks

- ARM server builds passed for Red Pitaya PNA, FFT and LED blinker, and ALPHA250
  FFT and tests-network. These cover both NEON and ordinary ARM server flags.
- The Kria KR260 hardware-only example also builds; it does not use the network
  reply path. The eight reply fixture cases separately pass ARM64 compilation
  and `qemu-aarch64-static` execution.
- All 54 host server tests passed using `g++-13` in `cross-armhf:24.04`.
- Mixed replies, driver locking and WebSocket tests passed with ASan/UBSan.
- New checks compare scalar/nested tuple wire bytes with `CommandBuilder`, and
  cover signed endpoints, bools, IEEE special values, complex numbers, quantities,
  referenced scalars, the 512-byte boundary, fallback paths, TX totals and
  disconnect/error propagation. Duration accounting checks both timestamp APIs.
- `git diff --check` passed. No FPGA build was required for these software edits.

Run the host tests using the CI command:

```sh
CXX=g++-13 python3 -m unittest discover -s server/tests -v
```

Build the standalone CPU benchmark from an SDK checkout with Eigen available:

```sh
g++-13 -std=c++23 -O3 -flto -fno-math-errno -pthread \
  -I. -Iserver/external_libs -I/usr/include/eigen3 \
  server/tests/benchmark_replies.cpp server/utilities/rate_tracker.cpp \
  -o tmp/reply-benchmark
./tmp/reply-benchmark
```

For board measurements, use the cross compiler and ARM flags above, link
statically and run the executable on the board. Compile the same benchmark
source against a baseline SDK checkout by placing its root first in the include
path; the session and rate headers must both come from that checkout.

## Hardware checks

All eight mixed/fixed reply fixture cases passed on the Red Pitaya itself,
including partial writes, interrupted writes, disconnects, descriptor limits,
borrowing and frame boundaries.

Live TCP and WebSocket comparisons passed exact reply-byte checks, both for
individual reads and 64-request batches. One comparison kept PNA acquisition
active at CIC 80 and polled status replies; spectra remained valid with no new
phase overflows, DMA errors, gaps or ring overruns. A second comparison queried
only the generator's scalar and settings RPCs, without starting PNA acquisition.
Network timings varied between baseline runs enough that these tests do not
establish a consistent end-to-end latency or throughput improvement.

The original board server, FPGA hash, analyzer settings, nominal LOs, precision
and native DAC words were restored and verified after each comparison. Only
Red Pitaya hardware was tested. Raw measurements, reversible scripts
and build logs are in the ignored local directory `tmp/shared-critical-paths/`.

## C++23 comparison

On 2026-10-09, the C++23 migration was compared with V1 `a5948d2d` on an
ALPHA250 using GCC 15, ARMv7 hard float, `-O3 -flto -fno-math-errno`, static
linking and CPU 1 affinity. The existing instrument stayed running. Both builds
used this benchmark; the baseline used C++20 and the candidate C++23 with
`std::byteswap`. Baseline/candidate/candidate/baseline runs each measured the
median of fifteen 100000-call batches. Averaging each pair of medians gave:

| Reply shape | C++20 ns | C++23 ns | Change |
| --- | ---: | ---: | ---: |
| array | 3578.96 | 3571.32 | -0.21% |
| vector | 3616.38 | 3614.19 | -0.06% |
| scalar | 1844.45 | 1843.80 | -0.04% |
| status | 1916.02 | 1917.72 | +0.09% |
| nested | 1921.42 | 1920.98 | -0.02% |

No measurable regression was observed in these reply workloads. Separate
code-generation probes produced identical `.text` bytes for all sixteen scalar
encode/decode functions (signed/unsigned 16/32/64-bit integers, float and double)
on ARMv7, AArch64 and x86-64 with both GCC 13 and GCC 15. This comparison covers
packing/accounting CPU cost, not end-to-end acquisition or network throughput.
Raw measurements and probe sources are in `tmp/cpp23-performance/` locally.

## Further C++23 use

The follow-up was compared with `38a8eb92` on the same ALPHA250. GCC 15
microbenchmarks used eleven batches per run and old/new/new/old ordering. Fresh
string allocation/copy with `resize_and_overwrite` reduced CPU time by 8–53%
over 8, 32, 256, 4096 and 65536-byte inputs (4 KiB: 3126 to 2058 ns).
Move-only callback dispatch took 15.07 ns versus 18.54 ns for `std::function`.
These isolate the changed mechanisms; they do not measure complete network
request latency or physical interrupt delivery.

The configuration container remains `std::map`. For 64 string keys, `flat_map`
lookup took 1172 versus 1165 ns, while inserting and erasing a key near the
beginning took 10643 versus 1864 ns. This workload did not justify switching.

Fixed-size command batches keep their existing decoded tuple representation.
The checked variable-length API exposes `std::expected` errors and uses
expected internally. Fixed-size scalar and batch decoding retain the original
implementation. A GCC 13 production-header decode/invoke/reply test
over a local UNIX socket measured 42419 ns for the baseline and 42547 ns for
the candidate, within observed between-run variation. Each binary ran eleven
20000-command batches pinned to CPU 1, in baseline/candidate/candidate/baseline
order. The instrument stayed active. Sources and raw measurements are stored
locally in `tmp/cpp23-phase2/`.
