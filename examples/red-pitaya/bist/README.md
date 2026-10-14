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

## Build and run

```sh
make -j4 CFG=examples/red-pitaya/bist/stemlab-125-14.mk
make deploy CFG=examples/red-pitaya/bist/stemlab-125-14.mk HOST=192.168.1.84
.venv/bin/pip install paramiko  # SSH transport for the reboot orchestrator
.venv/bin/python examples/red-pitaya/bist/client.py --host 192.168.1.84 --analog-mask 1
```

The client sets BIST as the boot instrument, reboots, waits for network and RPC
readiness, runs the suite, then stops and starts the normal instrument service.
The recovered server retains the report and verifies both UARTs and DMA again.
`BIST_SSH_PASSWORD` overrides the image's default root password; SSH keys also
work. `--ssh-known-hosts FILE` selects a host-key file. Use `--no-reboot` for
iteration without startup measurements. `--analog-mask` declares the connected
cables: `1` for DAC0/ADC0, `2` for DAC1/ADC1, `3` for both, or `0` for neither.
Unconnected channels record input noise and an explicit skipped result.

Open `http://192.168.1.84/` for the scorecard and live log. The webpage uses the
normal server's generated RPC methods over WebSocket. Closing it does not stop
validation. Reports are stored at `/var/lib/koheron/bist/<run-id>/` as
`report.json`, `scorecard.txt` and `log.txt`; the `latest` file identifies the
most recent run. The client downloads the same files to `tmp/bist-results/`.
Every measurement is persisted after completion; an interrupted worker leaves
its completed measurements available after service recovery.

Use `--compare previous/report.json` to compare OS runs. Comparison rejects
changed FPGA/server digests, BIST source identities, workload versions,
parameters or cable declarations and lists missing measurements. Startup
values identify their clock and origin; network readiness uses ICMP
independently of HTTP or RPC. Network/server readiness includes reboot shutdown
time, while boot readiness uses Linux monotonic uptime. DMA MB/s is payload
bytes divided by onboard transfer completion time, excluding pattern
preparation and verification.
UART throughput is the aggregate of both directions; baud is independently
checked using the PL frame counter and the converter clock is checked against
the onboard monotonic clock. UIO observation latency includes AXI register-read
overhead. RPC timing includes decode, generated dispatch, and reply handling
through a localhost TCP socket, independent of the external client.

## Fixed reference workload

The FPGA has a 4 KiB dual-port waveform BRAM, a 512-word capture FIFO, a full
duplex 32-bit AXIS / 64-bit DDR SG DMA, and two 64-byte FIFO UARTs. Both DACs
play the same 1024-word coherent waveform uploaded through MM2S; S2MM captures
both ADCs packed into one word. Capture decimates by eight to 15.625 MS/s,
keeping the DDR workload bounded. Analog checks fit a 3.90625 MHz tone and
report amplitude, DC offset, residual noise/distortion and clipping in ADC
codes; these are uncalibrated reference health checks.

The digital DMA workload uses 32 descriptors of 64 KiB, a distinct deterministic
pattern for every buffer reuse, full payload comparison and descriptor status
validation. Hardware counters independently observe transferred words,
packets, analog FIFO loss and digital sequence discontinuities. A snapshot
request latches all counters on one clock edge. The UART workers service RX/TX
causes before rearming level-sensitive UIO interrupts, and enable TX demand
interrupts only while unsent work remains. UART workers use `SCHED_FIFO` priority
1 to meet the 1.389 ms service budget of the 64-byte FIFOs; CPU, DMA and RPC work
uses normal scheduling. UART A/B workers are pinned to CPU 0/1 and begin traffic
only after both workers are ready. This fixed operating condition is recorded
in reports. Fractional 16x baud enables avoid integer-divider drift.

The isolated suite runs register/BRAM, DMA, UART, and generated RPC checks.
The combined workload runs one 8 MiB CPU/memory worker concurrently with DMA,
both UART workers and localhost RPC. Timed regions do not write report logs;
combined-workload file writes wait until all timed workers finish.
Request/reply uses persistent workers and one
outstanding 32-byte frame.
Cancellation is bounded by short IRQ waits; shutdown joins workers before
releasing mappings. The instrument uses UIO and the OS write-combined driver;
actual `/proc/self/maps` paths are reported, so SDK `/dev/mem` fallback fails
those checks. DMA exclusion is verified against both the boot device tree's
`no-map` reservation and `/proc/iomem`.

## Local verification

```sh
make check CFG=examples/red-pitaya/bist/stemlab-125-14.mk
.venv/bin/python -m pytest -q examples/red-pitaya/bist/tests python/tests/test_client_timeout.py
g++ -std=c++17 -Wall -Wextra -Werror examples/red-pitaya/bist/tests/test_measurements.cpp -o /tmp/bist-measurements
/tmp/bist-measurements
source /tools/Xilinx/2025.1/Vivado/settings64.sh
vivado -mode batch -source examples/red-pitaya/bist/tests/run_sim.tcl -tclargs tmp/bist-rtl
.venv/bin/python examples/red-pitaya/bist/tests/hardware_checks.py --host 192.168.1.84
```

The RTL simulations check FIFO framing/data, baud timing, skewed AXI channel
arrival, IRQ masking, resets, loopback backpressure, packet boundaries, BRAM
upload addressing, atomic snapshots and capture overflow diagnostics. Routed
FPGA timing is enforced by the instrument configuration.

## Measured validation

On 2026-10-14, board `192.168.1.84` (STEMlab 125-14 Gen1, Ubuntu 26.04.1,
Linux `6.18.0-xilinx`) completed run `1791936459949861` with all required
measurements present, 18 headline metrics and no failed checks. DAC0 was
connected to ADC0; channel 1 was explicitly skipped because it was unconnected.
The full reboot, validation and service-recovery sequence took 32.226 s.

| Measurement | Isolated | Combined load |
| --- | ---: | ---: |
| SG DMA payload throughput | 494.8 MB/s | 477.8 MB/s |
| Duplex UART aggregate throughput | 91.585 kB/s | 91.583 kB/s |
| UART request/reply median latency | 1424.6 us | 1429.4 us |
| Generated RPC throughput | 6628 requests/s | 251 requests/s |
| DMA/UART/RPC integrity errors | 0 | 0 |

Network and server readiness took 20.679 s and 20.691 s from the reboot request;
the server was ready at Linux uptime 11.928 s. The onboard suite took 4.884 s.
DAC0/ADC0 measured 4009 ADC codes peak, 50.48 dB tone-to-residual ratio, no
clipping and no capture FIFO loss. Controlled service restart took 0.516 s,
retained the report and recovered DMA and both UARTs. The browser's JSON
download matched the client report exactly.

Verification passed 147 host checks, 40 server socket/locking regressions,
measurement/client/overlay/timeout tests and both RTL simulations. Five
additional live runs passed; an intentionally incorrect channel-1 cable
declaration correctly failed only that analog check. Stopping during combined
transfers took 0.941 s, retained partial results and recovered the devices.
Routed timing passed at 125 MHz; the design uses 31.3% of LUTs and 10% of BRAM.
The board retains the report under `/var/lib/koheron/bist/1791936459949861/`.
