# Native runtime evidence — Red Pitaya

Tested on 2026-10-10 using GCC 15.2.0 and C++23. The reference Python API is V1
commit `902bd235c7582d31f0778b8b29b54e3e1864cd26`, including its direct systemd
status queries. The native implementation replaces the HTTP API, installer and
IP LED helper; the standard image removes Python, Flask and uWSGI.

## Physical board measurements

Red Pitaya at `192.168.1.85`, running Ubuntu 26.04.1 and Linux 6.18.0-xilinx.
Both API variants read the same installed FFT 0.3.0, live files and journals.
They run sequentially on loopback port 18085, with the production instrument,
nginx and uWSGI services still running. Three pairs alternate variant order.
Each pair includes 20 warm-up requests, 100 measured status requests and five
memory samples. No other test workload runs during these measurements.

| Median | Current V1 Python/uWSGI | Native C++23 | Change |
|---|---:|---:|---:|
| HTTP readiness after process launch | 2,439.19 ms | 50.47 ms | 48× faster |
| Combined process PSS | 24,161 KiB | 4,732 KiB | 80.4% lower |
| `/api/instruments/details` latency | 10.65 ms | 7.26 ms | 31.8% lower |

These are process-start and loopback measurements with warm filesystem caches,
not complete OS boot times or nginx/LAN latency. PSS includes the uWSGI master
and worker. Shared pages are apportioned across all running processes.

All 13 read-only route responses match in each of the three pairs, comparing
parsed JSON and exact raw-file content. Upload, run and delete are exercised
separately. [Raw measurements](native-runtime-red-pitaya.json) include every
latency sample, per-process memory samples, responses and executable hashes.

## Physical board functional tests

The compiled ARMhf executables pass **57 black-box tests** directly on the board:
HTTP routes, concurrent status, Unicode filename normalization, multipart
uploads and interrupted transfers, ZIP CRC/path/type validation, permissions,
rollback, socket activation and fragmented LED RPC. Service control in these
tests uses a private fixture. [Console output](native-runtime-red-pitaya-tests.txt).

A separate integration test uses the actual system manager, journal and nginx
with temporary units and private instrument directories. It verifies:

- `Type=notify` readiness, inherited Unix HTTP socket and nginx proxying.
- Multipart upload, successful activation and default archive protection.
- Invalid extraction leaving the running service untouched.
- Failed readiness restoring files and restarting the previous service.
- Journal timestamps/cursors, the first startup message after switching
  invocations, and following the restored invocation after rollback.
- API restart recovering identity, socket reuse and stopped-instrument status.
- Native LED RPC against the running production FFT server.

[Integration results](native-runtime-red-pitaya-integration.json) record the
checks, journal entries and production PIDs. All temporary units/processes are
removed. Production PIDs remain server **147**, uWSGI **144**, nginx **150**;
the production Python API file hash is unchanged. LED RPC succeeds; the LED
appearance is not visually inspected. Instrument activation tests use dummy
payloads and do not reprogram the FPGA.

## Build and image checks

- The default Ubuntu 26.04 builder compiles ARMhf and ARM64 binaries with GCC 15;
  an ARM64 QEMU version check also passes. There is no GCC 13 fallback.
- The dedicated Docker CI command passes **60 native tests**, including three
  injected filesystem failures; **10** growth tests, **5** rootfs settings
  tests and **2** private-chroot enablement tests.
- The OS regression suite passes **95 tests**, with the two root-only chroot
  tests skipped on the host and passed separately in Docker. All **3** real
  user-systemd ordering tests pass. Compiler-cache and APT-refresh checks pass.
- The assembled Red Pitaya image is **276 MiB**; its ZIP is **61.86 MiB**.
  Read-only inspection reports **190 MiB** of allocated rootfs content, three
  native executables, correct API enablement, and zero installed Python,
  libpython, uWSGI or cloud-guest-utils packages.
- The actual image API serves inventory and build-manifest HTTP responses in a
  read-only, Python-free ARM QEMU chroot. Image and board API hashes match.

[Build record](native-runtime-build-checks.json) contains versions, source and
image hashes. The local image is
`tmp/examples/red-pitaya/fft/red-pitaya-fft.zip`. Image assembly reused cached
instrument, FPGA, kernel and boot artifacts; the image's cached FFT version is
0.2.2. Those artifacts were not rebuilt or newly validated by this migration.
The generated image has **not been flashed or physically booted**. New-image
boot, FPGA programming, acquisition and ARM64 hardware tests remain unverified.

## Reproduction

The [test README](README.md) gives the Docker command. The board harnesses are
[red_pitaya_benchmark.py](red_pitaya_benchmark.py) and
[red_pitaya_integration.py](red_pitaya_integration.py); their docstrings list
staged inputs and ports. They require an existing Python-based V1 board for the
test harness, not for the native runtime. Run functional checks first, then the
benchmark alone. The reference API is at `902bd235:os/api/__init__.py` and its
adjacent `service_status.py`; obtain those files with `git show`.
