# BIST

`bist` (built-in self-test) is a compact reference instrument for validating V1 OS releases and
comparing their behaviour and performance. The first target is Red Pitaya Gen1
(STEMlab 125-14); ALPHA250 is the next intended target.

**Status: planned instrument.** This README is a design brief; implementation
details are left to the implementing agent.

## Scope and execution

- Use one board and one dedicated instrument, with a V1 OS image.
- Run in the normal SDK server, with generated RPC methods and the usual
  instrument packaging, default-at-boot loading and management lifecycle.
- Start the validation with a reboot and finish in less than three minutes.
  Bound waits and preserve partial results when a test fails or times out.
- Perform most measurements onboard. The external client handles orchestration
  and result retrieval; its performance and network throughput must not dominate
  the benchmark results.
- Run measurements asynchronously so the server remains responsive. Release
  resources cleanly when stopping the test or the instrument.
- Keep OS validation results separate from reference-instrument health checks.

## FPGA workload

Keep the design small and leave substantial FPGA headroom. Reuse datapaths and
measurement logic, with small BRAMs and FIFOs rather than large capture memories.
The implementing agent chooses the exact widths, depths and buffer sizes.

Include:

- SDK config/status IP for register integrity and access-performance tests.
- A small CPU-accessible BRAM for bulk access and integrity tests, also usable
  for DAC waveform playback.
- A representative full-duplex scatter-gather DMA path. Exercise descriptors,
  buffer reuse, interrupts, sustained transfers and exact digital data integrity.
- Both ADC/DAC channels, assuming **DAC0 is connected to ADC0** and **DAC1 to
  ADC1** with external cables. Reuse the DMA and playback paths for these checks.
- Shared timing and diagnostic counters for transfers, discontinuities and
  FIFO errors, with consistent snapshots.

## UART and UIO workload

Use two instances of a custom AXI-Lite UART core at **460800 baud**, with 8N1
framing and small TX/RX FIFOs. Cross-connect TX A to RX B and TX B to RX A inside
the PL. Each instance has its own UIO device and interrupt.

A userspace UART driver runs inside the normal instrument server. Two worker
threads, one per UART, communicate through the serial link and service their
devices through interrupts. Handle acknowledgement and UIO rearming correctly;
enable TX interrupts only while there is work to transmit.

Exercise short request/reply exchanges and simultaneous traffic in both
directions, with deterministic data and sequence checking. Repeat under CPU and
DMA load. Cover UIO mapping, repeated interrupt delivery, timeouts, shutdown and
restart. Verify baud timing as well as successful communication between the two
identical cores.

## OS and SDK coverage

Cover boot time, network readiness and server readiness, then isolated
measurements and a combined CPU/DMA/UART workload. Measure representative server
request handling and reply generation using onboard workloads.

Exercise runtime device-tree overlays, driver binding, UIO, write-combined
memory, read-only mapping properties and mapping/resource lifetime. Check that
DMA memory is reserved at boot and excluded from Linux allocations. Report the
mapping backend actually used: fallback to `/dev/mem` must not silently pass a
UIO or write-combined-driver check.

Include a controlled instrument stop/start to check overlay reapplication and
device recovery, preserving the run's results across the restart. Analog
loopback checks belong in the instrument-health results, separately from the OS
performance verdict.

## Reports and comparison

For each validation run, the running `bist` instrument must automatically
generate and store onboard a compact human-readable scorecard, machine-readable
results and a detailed timestamped log. Report generation is part of the
instrument's runtime behaviour. The external client retrieves these outputs;
a successful run is complete only when its report is available.

Aim for roughly **15–20 headline metrics**, backed by functional pass/fail
results and per-channel/per-workload detail.

The metrics should cover startup, register and bulk-memory access, SG DMA
throughput, UART throughput and latency, UIO interrupt observation latency,
server performance, and CPU/memory behaviour under load. Include integrity and
error counts; report latency distributions rather than averages alone.

Record board identity, OS/kernel/SDK and BIST versions, FPGA identity, test
parameters, units and relevant operating conditions. Keep the reference FPGA
and workloads stable for OS comparisons; flag changes that prevent a meaningful
comparison. Missing measurements must be explicit.

The log should explain failures with expected/observed results, timings and
diagnostic counters. Keep logging overhead out of timed measurements and avoid
large raw captures by default. Choose the final metric names, test sequencing,
budgets and acceptance thresholds during implementation and hardware validation.
