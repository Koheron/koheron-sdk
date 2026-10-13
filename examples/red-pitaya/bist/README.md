# BIST

`bist` (built-in self-test) is a compact reference instrument for validating V1
OS releases and comparing their behaviour and performance. Its reference
platform is Red Pitaya Gen1 (STEMlab 125-14), running a V1 OS image.

The instrument runs in the normal SDK server, with generated RPC methods and
the usual instrument packaging, default-at-boot loading and management
lifecycle. Most measurements execute onboard, so external client performance
and network throughput do not determine the benchmark results. The client
handles orchestration and retrieves the instrument's outputs.

## Validation run

A validation run begins with a reboot and completes in less than three minutes,
including report generation. It measures boot time, network readiness and
server readiness, then runs isolated checks and a combined CPU/DMA/UART
workload. Checks use bounded waits, with partial results retained on failure or
timeout.

Measurements run asynchronously while the server remains responsive. A
controlled instrument stop/start checks overlay reapplication and device
recovery, with results retained across the restart. Shutdown stops transfers,
joins worker threads and releases device mappings and resources.

OS validation results and reference-instrument health checks appear separately
in the report. The physical loopback connections are **DAC0 to ADC0** and
**DAC1 to ADC1**, using external cables.

## FPGA workload

The FPGA uses small BRAMs and FIFOs, shared datapaths and shared measurement
logic to leave substantial resource headroom. It contains:

- SDK config/status IP for register integrity and access-performance tests.
- A small CPU-accessible BRAM for bulk access, integrity tests and DAC waveform
  playback.
- A full-duplex scatter-gather DMA path covering descriptors, buffer reuse,
  interrupts, sustained transfers and exact digital data integrity.
- Both ADC/DAC channels, sharing the DMA and playback paths for analog loopback
  checks.
- Two custom AXI-Lite UART instances connected inside the PL.
- Shared timing and diagnostic counters for transfers, discontinuities and
  FIFO errors, with consistent snapshots.

## UART and UIO workload

The two UARTs operate at **460800 baud**, with 8N1 framing and small TX/RX FIFOs.
TX A connects to RX B and TX B connects to RX A inside the PL. Each instance has
its own UIO device and interrupt.

A userspace UART driver runs inside the normal instrument server. Two worker
threads, one per UART, communicate through the serial link and service their
devices through interrupts. Each worker services interrupt causes before
rearming UIO, with TX interrupts enabled only while transmission work is queued.

The test exchanges short request/reply messages and simultaneous traffic in
both directions, with deterministic data and sequence checking. It repeats
under CPU and DMA load and covers UIO mapping, repeated interrupt delivery,
timeouts, shutdown and restart. Baud timing is checked alongside successful
communication between the two identical cores.

## OS and SDK coverage

Onboard workloads measure representative server request handling and reply
generation, both in isolation and under combined load.

The checks cover runtime device-tree overlays, driver binding, UIO,
write-combined memory, read-only mapping properties and mapping/resource
lifetime. DMA memory checks verify its boot reservation and exclusion from
Linux allocations. The report identifies the mapping backend actually used;
fallback to `/dev/mem` is recorded as a failed UIO or write-combined-driver
check.

## Reports and comparison

For each validation run, `bist` automatically generates and stores onboard a
compact human-readable scorecard, machine-readable results and a detailed
timestamped log. These outputs come from the running instrument. A successful
run is complete when its report is available for retrieval.

The instrument webpage displays live, timestamped validation logs served by the
instrument over WebSocket. The live stream contains the same log retained with
the run. Validation and report generation continue independently of browser
connections.

The scorecard summarizes roughly **15–20 headline metrics** and functional
pass/fail results. Detailed results include per-channel and per-workload
measurements.

The metrics cover startup, register and bulk-memory access, SG DMA throughput,
UART throughput and latency, UIO interrupt observation latency,
server performance, and CPU/memory behaviour under load. Results include
integrity and error counts, plus latency distributions.

Each report records board identity, OS/kernel/SDK and BIST versions, FPGA
identity, test parameters, units and relevant operating conditions. OS
comparisons use a fixed reference FPGA and fixed workloads, with changes that
affect comparability and missing measurements identified explicitly.

The detailed log explains failures with expected and observed results, timings
and diagnostic counters. Logging is decoupled from timed measurements, and raw
captures are limited to failure diagnostics.
