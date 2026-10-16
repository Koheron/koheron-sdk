# Hardware validation — 2026-10-16

Instrument: **bist-full 0.1.0**, workload 1, original STEMlab 125-14.
Host Ubuntu 24.04; Vivado/Vitis 2025.1; ARM GCC 15. Board at 192.168.1.85,
DAC0 connected to ADC0, channel 1 unconnected. OS image
`20261015.195921.a8061a0f1cc6`, Ubuntu 26.04.1, kernel `6.18.0-xilinx`.
SDK base under test: `d030a8a32c9b`; the new reference implementation is
identified separately by its source and binary digests below.

## Build and host checks

- New FPGA bitstream built and routed with timing enforcement enabled.
  Setup slack **0.538 ns**, hold slack **0.021 ns**, no negative slack.
- **6297/17600 LUTs (35.78%)**, **10353/35200 registers (29.41%)**,
  **8/60 BRAM tiles (13.33%)**, **0 DSPs**.
- Production ARM server and web package built and deployed through SDK commands.
- `make check CFG=examples/red-pitaya/bist-full/stemlab-125-14.mk`: **161 host checks passed**.
- Combined reference-client pytest run: **124 passed**. CI includes this suite.
- RTL simulation passed elastic AXIS stability, byte tails, independent source,
  checker corruption, malformed KEEP, early/missing TLAST, ADC overflow,
  reset while stalled, IRQ clear and snapshot wrap.

## Full hardware validation

All three runs below used the final implementation, a fresh reboot, different
pattern seeds, the 60-second end-to-end budget and the live webpage connected.
Every run produced **175 PASS, 0 FAIL, 1 SKIP**, comprising 135 onboard and
41 host checks. The only skip is the absent channel-1 analog cable. All seven
coverage groups and the complete required-case manifest were present.

| Run | End-to-end | Onboard suite | Observer reads | DMA payload MB/s | Duplex UART kB/s |
| --- | ---: | ---: | ---: | ---: | ---: |
| 1 | 29.253 s | 5.745 s | 0 | 492.567 | 91.574 |
| 2 | 29.669 s | 5.734 s | 0 | 492.834 | 91.564 |
| 3 | 29.636 s | 5.745 s | 0 | 493.168 | 91.576 |

End-to-end includes reboot, ICMP/RPC readiness, onboard checks, protocol probes,
service/overlay reload and fresh DMA/UART recovery. The SDK `make test` entry
point also passed during validation. DMA MB/s counts one direction's payload
per transfer-completion time, excluding preparation/comparison; UART is the
sum of both directions. These are descriptive results, not SDK regression
verdicts or analog calibration claims.

## Fault and recovery validation

The separate recovery script passed every case within its own 60-second budget:

- Onboard budget expiration leaves a failing, partial report.
- Starting an already-active suite is rejected.
- Cancellation during combined traffic wakes two completion waiters.
- Active service stop retains a cancelled report and devices recover.
- SIGKILL during combined traffic restores an interrupted report; devices recover.
- A truncated checkpoint falls back with `storage_ok=false`.
- Two corrupt generations produce an explicit corrupt report with no invented checks.
- Restoring the saved checkpoint pair restores checks and host results exactly.

Measured cancellation: **69.1 ms**; active stop: **186.2 ms**.
The entire recovery script took **30.459 s**.

Additional negative wiring control: declaring channel 1 connected while leaving
it unplugged fails all seven channel-1 sweeps. The other hardware and SDK checks
remain available; the overall full validation correctly fails.

Expected faults also passed within each normal suite: bad descriptors, input
and post-checker corruption, early/missing packet end, masked DMA IRQ, RX/TX
UART overruns and capture overflow. Every corresponding recovery probe passed.

## Identity and evidence

- `source_sha256`: `73d47d45ce9dc3c7a99a5f2c46a1de2b1184890d5eb8f061c58ed33b2580a21d`
- `server_sha256`: `7d9a23b29f49f52f5cac3810a2eb7a269676d4b4dd5d1c95becf21347e81751f`
- `fpga_sha256`: `0c665d862f7fdc560f5aa626fdfe0f1309d87411bff5d516ac1ccbe34c46c3d3`

Raw host reports, scorecards, logs, recovery fixtures and summaries are retained
locally under `tmp/bist-full-evidence/verified/`; build, simulation and host-check
logs are under `tmp/bist-full-*.log` and `tmp/bist-full-evidence/`.
The reports retain per-run boot ID, FPGA DNA, OS/image provenance and client hash.

These runs cover this board and fixture. They do not certify the unconnected
analog channel, external expansion wiring/clocking, every DDR cell, SD power
loss, Ethernet line-rate capacity or extended thermal endurance.
