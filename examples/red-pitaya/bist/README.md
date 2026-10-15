# BIST

`bist` (built-in self-test) is a compact reference instrument for validating and
comparing V1 OS releases on Red Pitaya Gen1 (STEMlab 125-14).

It runs in the normal SDK server, with generated RPC methods, standard
instrument packaging, default-at-boot loading and the usual management
lifecycle. Most measurements run onboard, independently of client performance
and network throughput. The client orchestrates the run and retrieves results.

## Validation run

A run begins with a reboot and ends with its report ready for retrieval within
30 seconds. It measures boot time, network readiness and server
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
The client verifies the new boot ID over RPC, so SSH is needed only to request
the reboot. Completed measurements are downloaded before the service restart,
so a later lifecycle failure still leaves the suite's report on the host.
`BIST_SSH_PASSWORD` overrides the image's default root password; SSH keys also
work. `--ssh-known-hosts FILE` selects a host-key file. Use `--no-reboot` for
iteration without startup measurements. `--analog-mask` declares the connected
cables: `1` for DAC0/ADC0, `2` for DAC1/ADC1, `3` for both, or `0` for neither.
Unconnected channels record input noise and an explicit skipped result.

Open `http://192.168.1.84/` for run metadata, the scorecard and log. The webpage uses the
normal server's generated RPC methods over WebSocket. Closing it does not stop
validation. BIST 1.5 clients and the webpage wait for completion without polling
reports during any profile. A separate `BistCompletion` SDK driver sleeps until
the worker finishes, leaving the measured `Bist` driver free to serve benchmark
RPCs and cancellation. Results and logs appear after completion; cancellation
and service shutdown remain responsive. The updated client requires BIST 1.5
or newer. Reports are stored at `/var/lib/koheron/bist/<run-id>/` as
`report.json`, `scorecard.txt` and `log.txt`; the `latest` file identifies the
most recent run. The client downloads the same files to `tmp/bist-results/`.
The scorecard identifies the instrument and SDK versions, Ubuntu release,
kernel, Koheron OS image/build/source and reference digests. JSON metadata
includes structured `os`, `os_image` and `os_manifest` objects, with the raw
OS release and instrument manifest retained. The webpage also exposes SDK
source, boot ID, FPGA identity/DNA and build digests. These describe the saved
run and remain available after a restart; absent fields display as unavailable.
Every measurement is persisted after completion; an interrupted worker leaves
its completed measurements available after service recovery.

Use `--compare previous/report.json` to compare OS runs. Comparison rejects
changed FPGA/server digests, BIST source identities, workload versions,
parameters or cable declarations and lists missing measurements. Comparisons
also reject absent identities, even when both reports omit the
same field. Null or non-finite measurement values are identified explicitly
and have no ratio; zero reference values also have no ratio. Startup
values identify their clock and origin; network readiness uses ICMP
independently of HTTP or RPC. Network/server readiness includes reboot shutdown
time, while boot readiness uses Linux monotonic uptime. Isolated DMA MB/s is payload
bytes divided by onboard transfer completion time, excluding pattern
preparation and verification. Combined DMA reports verified payload completed
inside the shared window divided by that window. Its `engine_MB_s` field
retains transfer-only throughput across all completed transfers. The combined
rate therefore includes software preparation, verification and scheduling;
these two rates measure different parts of the workload.
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
after worker initialization. Each service batch pauses for 100 us to give
normal workers CPU time while staying within the FIFO service budget. This
fixed operating condition is recorded in reports. Fractional 16x baud enables avoid integer-divider drift.

The isolated suite runs register/BRAM, DMA, UART, and generated RPC checks.
The combined workload runs one 8 MiB CPU/memory worker concurrently with DMA,
both UART workers and localhost RPC. Timed regions do not write report logs;
combined-workload file writes wait until all timed workers finish.
Request/reply uses persistent workers and one outstanding 32-byte frame.

Workload version 3 prepares memory, UART scheduling and RPC dispatch before a
shared start gate. CPU, DMA, RPC and both UART workers use one epoch and a
common measurement window. CPU/DMA/RPC load continues while UART traffic and
request/reply verification drain; this extra time is reported separately.
CPU load is pinned to CPU 1; DMA and the RPC client use CPU 0. SDK dispatch
workers and kernel interrupt routing retain their normal policies. The report
records start offsets, preparation failures and external report-read traffic.

Combined throughput counts work completed within the common window. RPC
latency includes the full completion time of every call started inside that
window, including late replies. Latencies cover the whole observation
population using bounded histograms, with exact min/max, 0.25 us bins below
16 us and a maximum 0.78125% midpoint error above it. DMA and UART request/reply
samples remain exact. All distributions use nearest-rank percentiles: p95
requires 100 observations, p99 requires 1000; unavailable tails are `null`.

Snapshot acknowledgement is mandatory. Capture DMA, descriptor lengths and
FIFO loss are checked independently of cable declarations. Every row has a
`pass`, `fail`, `skipped` or `incomplete` status. Cancelled benchmark values are
`null`, with completed counts retained. Logs include both UTC and a monotonic
`steady_ms` timestamp, interpreted with the saved boot ID.

Checkpoint generations contain all report fields together, with bounded
lengths and a CRC. Atomic replacement preserves a previous generation;
terminal checkpoints are synced to disk. Restart restores one validated
checkpoint, marks an unfinished run interrupted, and reports failed storage
when it must fall back. Two invalid generations yield an explicit corrupt
report. JSON, scorecard and log files are derived exports.

## Profiles and performance baselines

The default `health` profile uses 0.5 s isolated DMA, 0.3 s for each isolated
RPC benchmark, 32768 bytes per UART direction, and a 0.5 s common window. It
keeps the original 30-second full validation budget. `benchmark` explicitly
uses a 60-second client budget: 2 s isolated DMA and common windows, 1 s per
isolated RPC check, and 131072 UART bytes per direction. It collects more
samples for performance comparisons. `diagnostic` adds bounded payload
corruption, a zero-length DMA descriptor, a stalled stream, a masked UIO IRQ,
and a missing snapshot acknowledgement; every injected failure must be
detected and followed by verified recovery.

```sh
.venv/bin/python examples/red-pitaya/bist/client.py --no-reboot --profile diagnostic
for run in 1 2 3 4; do
  .venv/bin/python examples/red-pitaya/bist/client.py --no-reboot --profile benchmark --output tmp/bist-ref-$run
done
.venv/bin/python examples/red-pitaya/bist/client.py --no-reboot --profile benchmark \
  --output tmp/bist-ref-5 --save-baseline tmp/bist-baseline.json \
  --reference-report tmp/bist-ref-1/report.json --reference-report tmp/bist-ref-2/report.json \
  --reference-report tmp/bist-ref-3/report.json --reference-report tmp/bist-ref-4/report.json
.venv/bin/python examples/red-pitaya/bist/client.py --no-reboot --profile benchmark \
  --baseline tmp/bist-baseline.json --output tmp/bist-comparison
```

Functional integrity and performance regression are separate outcomes.
Baselines require distinct, complete, functionally healthy reports with matching
reference identities, parameters, board and FPGA DNA. A metric needs at least
five runs. Combined windows must record zero report/log observer requests;
disturbed runs cannot seed a baseline or receive a performance verdict, even
when functional integrity passes. Schema-1 baselines predate this validation
and must be rebuilt from quiet runs; they evaluate as inconclusive. Limits use
the median and the larger of the configured tolerance
(default 10%) or three scaled MADs (`3 × 1.4826 × MAD`), with a floor of one completed
batch for combined DMA and CPU throughput. This accounts for the resolution
of counting fixed-size batches inside a window. Saturation RPC latency remains
descriptive in the report: its competing workers' achieved load varies, so
baseline verdicts use RPC capacity and isolated latency instead. Use the
controlled SDK comparison below for latency at a fixed offered load. Excessively noisy or
insufficient baselines remain inconclusive. Throughput regressions fall below
the limit; latency regressions exceed it. Missing values and incompatible
identities never pass. OS/kernel/environment provenance is retained for each
reference run. The saved report and webpage show the separate performance
outcome and individual comparisons. A crossed limit identifies slower
measurements against the reference; repeat the comparison before attributing
that difference to an OS change. `--compare` remains a descriptive ratio
comparison with one previous report, without a regression verdict.

Cancellation is bounded by short IRQ waits; shutdown joins workers before
releasing mappings. The instrument uses UIO and the OS write-combined driver;
actual `/proc/self/maps` paths are reported, so SDK `/dev/mem` fallback fails
those checks. DMA exclusion is verified against both the boot device tree's
`no-map` reservation and `/proc/iomem`.

## Controlled SDK comparison

BIST 1.3.0 adds a `comparison` profile (workload 4) alongside the existing
workload-3 health, benchmark and diagnostic profiles. Use it to compare SDK
request latency while holding the offered background load constant. The
saturation benchmark remains useful for maximum throughput; its combined
latency also depends on how much CPU/memory work the other workers achieve.

Each selected case runs for two seconds at **600 verified RPC requests/s**, with
one outstanding request. Cases are idle, memory, DMA, and mixed memory/DMA.
Memory performs 16 MiB/s of logical reads+writes in 16 KiB batches over an 8 MiB
working set. DMA transfers and verifies 1 MiB/s of payload in 4 KiB duplex
transactions (payload counts one direction). UART stays idle. CPU affinity
matches the saturation test: memory on CPU1; DMA and the loopback TCP RPC client
on CPU0; normal SDK server scheduling. These are modest, fixed loads, not peak
bandwidth tests or measurements of physical DRAM traffic.

Releases use absolute times. Expired slots are skipped rather than replayed in
a burst. Reports retain requested, started, completed and missed counts, errors,
affinity failures, and three distributions: client wall RTT, release lateness,
and time from planned release through reply validation. This makes scheduling
delay visible even when an RPC starts late. Percentile sample minimums remain
100 for p95 and 1000 for p99. A request that finishes after the window is sampled
but excluded from completed load. This sequential probe cannot measure queued,
open-loop overload; missed slots make unmet demand explicit.

A case has valid comparison quality only with zero integrity/affinity errors,
at least 98% of each active worker's requested operations completed, and zero
BIST report/log/status observer calls during measurement. The webpage and
Python client wait quietly for completion during every profile.
An old cached webpage or another observer makes the affected run inconclusive.
Other applications and unrelated SDK traffic still need to be controlled by
the operator. Functional integrity and comparison quality are separate results.

Run all four cases from the webpage's **RPC comparison · controlled load**
profile, or without rebooting:

```sh
.venv/bin/python examples/red-pitaya/bist/client.py --host 192.168.1.84 \
  --no-reboot --profile comparison --output tmp/bist-controlled
```

To automate an SDK comparison, build the FPGA once with the normal configuration,
then supply two SDK Git revisions. The command builds the **same current BIST
source** against each revision in temporary detached worktrees, reusing the
validated FPGA outputs. It builds software only, saves both packages, and
removes its own build worktrees. Build logs and resolved revisions are retained.

```sh
.venv/bin/python examples/red-pitaya/bist/compare_sdk.py --host 192.168.1.84 \
  --baseline-ref e941c90f --candidate-ref origin/V1 \
  --output tmp/bist-sdk-comparison
```

`--fpga-cache PATH` selects another validated output directory containing
`bist.bit`, `bist.bit.bin`, `pl.dtbo` and `fpga/bist.xsa`. Both builds require the
normal SDK cross-compilation environment. For fast reruns, use
`--baseline PATH/bist.zip --candidate PATH/bist.zip` with the saved packages.
Either side can independently use a revision or a prebuilt package.

The default is five pairs of idle and mixed cases, alternating A/B then B/A.
`--cases idle memory dma mixed` selects all cases. The command prints
**provisional results after the first pair**, saves each raw report immediately,
and updates `comparison.json` after each pair. It verifies the deployed server
and FPGA hashes against the intended package. Packages must share BIST source,
FPGA, workload, parameters, board identity, boot, OS image and kernel; SDK/server
identity may differ. Existing OS-baseline comparisons retain their stricter
server-identity requirement.

Each valid pair must also achieve loads within 2% of each other. The result
reports paired median candidate/reference ratios and descriptive 95% bootstrap
intervals (10,000 resamples, fixed seed), conservatively expanded for histogram
resolution. The unexpanded bootstrap interval is retained separately. At least
five valid pairs are required
for a verdict. An interval entirely below/above the default ±5% band indicates
improvement/regression; an interval contained inside the band is within
tolerance; overlapping intervals remain inconclusive. Missing percentiles stay
visible as inconclusive. These per-metric intervals do not adjust for multiple
comparisons; repeat important findings with more pairs before attribution.

The command **restores the baseline package in `finally`**, including on failure
or interruption. Use `--restore-package PATH/bist.zip` to restore a different
known package. Raw results survive measurement or restoration failure; a failed
restoration additionally writes `restore-error.json`. Keep the chosen restore
archive before starting. The command does not reboot the board, set the default
instrument, or run service-recovery checks inside measured windows. Exit 2 means
incompatible, invalid or insufficient paired runs; valid runs may still have
statistically inconclusive individual metrics.

## Upload ownership comparison

BIST 1.4.0 adds controlled workload 5 for SDK argument ownership. Two cases
send **128 KiB at 100 requests/s for two seconds**, with one outstanding RPC:

- `upload_value` passes a vector by value. The receiver validates every word,
  then retains ownership of that vector as an uploaded buffer.
- `upload_reference` passes the same payload by const reference and validates
  every word without retaining it. It controls for changes outside ownership
  transfer.

Two deterministic payloads with different seeds alternate between requests.
They are constructed before the common epoch, and a warmup request precedes
timing. Acknowledgements identify the seed and payload length; corruption at
any word fails validation. No UART, memory-load or DMA worker runs in these
cases. The by-value endpoint retains at most one 128 KiB buffer. Timing includes
socket I/O, SDK decode/invoke, receiver validation and reply checking.

Each window offers 200 requests, enough for p95; p99 remains unavailable below
its existing 1000-sample minimum. Missed slots, achieved load, observer checks,
paired ordering and uncertainty follow the controlled comparison rules above.
The upload target appears under `parameters.controlled.upload`; the scalar
comparison defaults remain recorded alongside it.

```sh
.venv/bin/python examples/red-pitaya/bist/client.py --host 192.168.1.84 \
  --no-reboot --profile upload --output tmp/bist-upload
.venv/bin/python examples/red-pitaya/bist/compare_sdk.py --host 192.168.1.84 \
  --baseline-ref origin/V1 --candidate-ref YOUR_SDK_REVISION \
  --cases upload_value upload_reference --output tmp/bist-upload-comparison
```

The webpage's **Upload comparison · ownership** profile runs both cases.
`start_comparison` accepts bits 4 and 5 for value/reference uploads (mask 48
selects both). Masks containing upload cases identify workload 5. Health,
saturation and diagnostic profiles retain workload 3; comparisons containing
only the original four cases retain workload 4. Default SDK comparisons still
select idle and mixed scalar cases. Overlay the same current BIST source on
both SDK revisions, as the comparison command does automatically.

## Local verification

```sh
make check CFG=examples/red-pitaya/bist/stemlab-125-14.mk
.venv/bin/python -m pytest -q examples/red-pitaya/bist/tests python/tests/test_client_timeout.py
g++ -std=c++23 -Wall -Wextra -Werror examples/red-pitaya/bist/tests/test_measurements.cpp -o /tmp/bist-measurements
/tmp/bist-measurements
g++ -std=c++23 -Wall -Wextra -Werror examples/red-pitaya/bist/tests/test_metadata.cpp -o /tmp/bist-metadata
/tmp/bist-metadata
g++ -std=c++23 -Wall -Wextra -Werror -pthread examples/red-pitaya/bist/tests/test_checkpoint.cpp -o /tmp/bist-checkpoint
/tmp/bist-checkpoint
g++ -std=c++23 -Wall -Wextra -Werror -pthread examples/red-pitaya/bist/tests/test_pacing.cpp -o /tmp/bist-pacing
/tmp/bist-pacing
g++ -std=c++23 -Wall -Wextra -Werror -pthread examples/red-pitaya/bist/tests/test_upload.cpp -o /tmp/bist-upload
/tmp/bist-upload
source /tools/Xilinx/2025.1/Vivado/settings64.sh
vivado -mode batch -source examples/red-pitaya/bist/tests/run_sim.tcl -tclargs tmp/bist-rtl
.venv/bin/python examples/red-pitaya/bist/tests/hardware_checks.py --host 192.168.1.84
.venv/bin/python examples/red-pitaya/bist/tests/recovery_checks.py --host 192.168.1.84
```

The RTL simulations check FIFO framing/data, baud timing, skewed AXI channel
arrival, IRQ masking, resets, loopback backpressure, packet boundaries, BRAM
upload addressing, atomic snapshots and capture overflow diagnostics. Routed
FPGA timing is enforced by the instrument configuration.

## Measured validation

BIST 1.4.0 / controlled workload 5 was measured on `192.168.1.84` on
2026-10-15 (Europe/Paris), with DAC0/ADC0 connected, channel 1 unconnected,
Ubuntu 26.04.1 / Linux 6.18.0-xilinx and the unchanged FPGA. Five alternating
pairs compared SDK `8ba70a79` with `e0872822` (move decoded value arguments),
using identical BIST source on both SDK revisions. Each upload case offered
200 requests per two-second window, with no observation traffic.

| Controlled RPC RTT | Baseline | Move arguments | Paired median change | 95% ratio interval | ±5% verdict |
| --- | ---: | ---: | ---: | --- | --- |
| Upload by value p50 | 1656 us | 1336 us | −20.1% | 0.756–0.819 | Improvement |
| Upload by value p95 | 1704 us | 1368 us | −19.7% | 0.746–0.825 | Improvement |
| Upload by const reference p50 | 1256 us | 1256 us | +1.3% | 0.926–1.029 | Inconclusive |
| Idle scalar p50 | 139 us | 139 us | 0.0% | 0.970–1.031 | Within tolerance |
| Mixed scalar p50 | 163 us | 163 us | 0.0% | 0.984–1.028 | Within tolerance |

Baseline/candidate columns are medians across runs; changes are medians of the
paired ratios. Intervals include worst-case histogram resolution. The scalar
controls used a separate five-pair comparison. All twenty comparison runs had
valid matched loads and functional PASS. Upload p99 was unavailable because
each case collected only 200 samples. The reference control was inconclusive;
the measured gain applies to this 128 KiB ownership-transfer workload.

The upload CLI profile passed in **7.343 s**, including report retrieval and
service restart/recovery. Thirty-two hardware acknowledgement checks covered
both endpoints and seeds, rejecting corruption at the first/middle/last word,
wrong seeds, short payloads and empty payloads. Invalid masks and concurrent
starts were rejected; upload cancellation completed in **53 ms**, retained an
incomplete report, and a subsequent health validation passed. Local validation
passed 151 host checks, 85 BIST/Python checks, five C++ helper checks and 57
socket/lifecycle checks. The ownership regression test fails on the baseline
SDK's extra vector copy and passes for TCP, Unix and WebSocket with the move
change. The production ARM build passed with warnings as errors.
The browser upload profile also passed on the baseline SDK in **5.543 s**
onboard, with 200/200 requests in both cases and zero observer calls; its JSON
download matched the RPC report exactly. The board was left on the baseline
SDK with BIST 1.4.0 after health and service-recovery validation.

BIST 1.3.0 / controlled workload 4 was measured on `192.168.1.84` on
2026-10-15 (Europe/Paris), on the same Ubuntu 26.04.1 / Linux 6.18.0-xilinx image
and unchanged FPGA as the workload-3 validation below. The four-case browser
run completed in **9.625 s** onboard, with 1200/1200 verified RPCs in each case,
1024/1024 memory batches and 512/512 DMA transactions where enabled, and zero
observer calls. The browser JSON matched the client report exactly. A service
restart retained the entire report unchanged. Invalid masks and concurrent
starts were rejected; cancellation during a mixed window completed in **63 ms**.
The existing health profile passed in 3.433 s onboard and 4.429 s through client
report retrieval and service recovery.

Five alternating pairs compared SDK `e941c90f` (rate tracking) with `40bd5150`
(rate tracking removed), using identical workload-4 source and FPGA. All loads
were valid; each idle+mixed suite took 5.570–5.585 s onboard.

| Controlled RPC RTT | Rate tracking | Removed | Paired median change | ±5% verdict |
| --- | ---: | ---: | ---: | --- |
| Idle p50 | 147 us | 137 us | −6.8% | Inconclusive |
| Mixed p50 | 173 us | 163 us | −5.8% | Inconclusive |

The resolution-expanded 95% ratio intervals were 0.918–0.960 and 0.917–0.957,
respectively. Both suggest lower median latency, while still crossing the 0.95
boundary for claiming a reduction beyond the configured 5% tolerance. p95/p99
RTT verdicts were also inconclusive. These controlled results do not reproduce
the apparent median-latency increase in the older saturation comparison.

A separate five-pair comparison of the **same package against itself** produced
0.0% paired median change in both cases, with both p50s within tolerance and no
improvement/regression verdicts among the 18 reported comparisons. Prebuilt
packages produced the first provisional pair after **16.6 s**; all five paired
measurements finished after **84.9 s**, before final restoration. Local
verification passed 151 host checks, 85 BIST/Python checks and four C++ helper
checks. Production ARM builds passed with warnings as errors. FPGA SHA256 remains
`6384e43ad9c4382d7ff485726805455dadedce0487f50d7bad8af6cea3229e86`.

BIST 1.2.0 / workload 3 was measured on `192.168.1.84` on 2026-10-14, with
DAC0/ADC0 connected and channel 1 unconnected. Two final cold-boot validations
of the committed instrument passed in **27.689 s** and **28.553 s**, including
report downloads and service recovery. The onboard suites took **3.431 s** and
**3.383 s**. Both reports contained all 33 required measurements, 18 headline
metrics, functional PASS and healthy storage. Their reference comparison was
compatible. The validated FPGA image is unchanged.

Five benchmark runs took 10.567–10.717 s onboard. Isolated DMA measured
494.1–495.0 MB/s; combined verified payload measured 5.243 MB/s including
preparation, verification and scheduling. Combined RPC measured 1829.5–2117.5
requests/s. UART retained approximately 91.58 kB/s aggregate with no integrity
errors. A sixth same-OS comparison crossed the combined RPC p95/p99 limits;
CPU throughput was classified noisy. Functional checks still passed. The DMA
comparison accounts for one-transfer resolution, so a single batch at the
window boundary does not become a regression. These results illustrate why
performance alerts are separate and should be repeated before diagnosing an
OS change.

The diagnostic profile passed all five injected failures and verified recovery,
taking 3.829 s onboard. SIGKILL during the combined workload restored an
interrupted report and recovered DMA/UART after overlay reload. Truncated
checkpoint fallback and rejection of two corrupt generations passed. Five
additional health runs passed with a maximum responsive ping of 3.73 ms;
incorrect channel-1 cable declaration failed only that analog check. Active
shutdown took 0.353 s and retained incomplete results.

Local verification passed 150 host checks, 67 BIST/Python checks and all three
C++23 helper checks. The instrument was built with GCC 15 and warnings as
errors. Browser verification covered metadata, profiles, separate performance
results and a JSON download exactly matching the client report, including the
64-bit checksum stored as a string.

The earlier BIST 1.1.0 / workload 2 completed a full reboot validation on 2026-10-14 in
28.325 s, including report download and service recovery. The onboard suite
took 3.494 s. Isolated DMA completed seven verified buffer generations at
493.0 MB/s; combined DMA completed four at 474.5 MB/s. The isolated RPC checks
verified 1992 ping replies and 1582 bulk replies. Duplex UART measured
91.586/91.583 kB/s aggregate, and DAC0/ADC0 measured a 52.12 dB
tone-to-residual ratio. All 31 measurements were present, with no failed checks.
The FPGA digest matches the original validated design.

Five additional live runs passed with a maximum responsive ping of 45.3 ms.
Missing-cable detection and shutdown during active transfers passed; stopping
took 0.964 s and retained the partial report. Local checks passed 149 host
tests, 52 BIST/Python checks and both C++ helper checks. Run metadata identifies
instrument `bist` 1.1.0, SDK 1.0, Ubuntu 26.04.1 LTS, kernel `6.18.0-xilinx`
and OS image build `20261010.144242.dc0ae370d0f7`.

The original BIST 1.0.0 / workload-1 evidence below is retained for reference;
comparison explicitly rejects it as a workload-2 baseline.
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
