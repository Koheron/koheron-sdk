# BIST

`bist` (built-in self-test) is a compact reference instrument for validating and
comparing V1 OS releases on Red Pitaya Gen1 (STEMlab 125-14).

It runs in the normal SDK server, with generated RPC methods, standard
instrument packaging, default-at-boot loading and the usual management
lifecycle. Most measurements run onboard, independently of client performance
and network throughput. The client orchestrates the run and retrieves results.

## Validation run

A run begins with a reboot and ends with its report ready for retrieval in less
than three minutes. It measures boot time, network readiness and server
readiness, then runs isolated checks and a combined CPU/DMA/UART workload.
Checks have bounded waits and retain partial results on failure or timeout.

Asynchronous measurements keep the server responsive. A controlled instrument
stop/start checks overlay reapplication and device recovery while preserving
results. Shutdown stops transfers, joins workers and releases mappings and
other resources.

The report separates OS validation from reference-instrument health. External
cables connect **DAC0 to ADC0** and **DAC1 to ADC1**.

## FPGA workload

Small BRAMs and FIFOs, shared datapaths and measurement logic leave substantial
FPGA headroom. The design contains:

- SDK config/status IP for register integrity and access-performance tests.
- A small CPU-accessible BRAM for bulk access, integrity tests and DAC waveform
  playback.
- A full-duplex scatter-gather DMA path covering descriptors, buffer reuse,
  interrupts, sustained transfers and exact digital data integrity.
- Both ADC/DAC channels, sharing DMA and playback paths for analog loopback.
- Two internally connected UARTs.
- Timing and diagnostic counters for transfers, discontinuities and FIFO
  errors, with consistent snapshots.

## UART and UIO workload

Two custom AXI-Lite UARTs operate at **460800 baud**, with 8N1 framing and small
TX/RX FIFOs. TX A connects to RX B and TX B to RX A inside the PL. Each UART has
its own UIO device and interrupt.

A userspace UART driver runs two server worker threads, one per UART, which
communicate through the serial link. Each services interrupt causes before
rearming UIO; TX interrupts are enabled only while transmission work is queued.

Tests exchange short request/reply messages and simultaneous traffic in both
directions, checking deterministic data and sequences, both idle and under CPU
and DMA load. Coverage includes UIO mapping, repeated interrupt delivery,
timeouts, shutdown and restart. Baud timing is checked independently of
communication success.

## OS and SDK coverage

Server benchmarks measure representative request handling and reply generation,
in isolation and under combined load.

Checks cover runtime device-tree overlays, driver binding, UIO, write-combined
memory, read-only mappings, and mapping/resource lifetime. DMA memory checks
verify its boot reservation and exclusion from Linux allocations. The report
identifies the actual mapping backend; fallback to `/dev/mem` fails the UIO or
write-combined-driver check.

## Reports and comparison

For each run, `bist` automatically generates and stores onboard:

- A human-readable scorecard with roughly **15–20 headline metrics** and
  functional pass/fail results.
- Machine-readable results with per-channel and per-workload detail.
- A detailed timestamped log of expected and observed results, timings and
  diagnostic counters.

The instrument webpage displays the same log live, served by `bist` over
WebSocket. Validation and report generation continue independently of browser
connections.

Metrics cover startup, register and bulk-memory access, SG DMA throughput, UART
throughput and latency, UIO interrupt observation latency, server performance,
and CPU/memory behaviour under load. Results include integrity and error counts
and latency distributions.

Reports record board and FPGA identity, OS/kernel/SDK and BIST versions, test
parameters, units and operating conditions. OS comparisons use a fixed
reference FPGA and fixed workloads; missing measurements and changes affecting
comparability are identified explicitly.

Logging is decoupled from timed measurements, and raw captures are limited to
failure diagnostics.
