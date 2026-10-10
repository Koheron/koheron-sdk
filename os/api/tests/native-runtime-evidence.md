# Native runtime evidence — Red Pitaya

Tested on 2026-10-10 using GCC 15.2.0 and C++23. The reference Python API is V1
commit `902bd235c7582d31f0778b8b29b54e3e1864cd26`, including its direct systemd
status queries. The native implementation replaces the HTTP API, installer and
IP LED helper; the standard image removes Python, Flask and uWSGI.

## Matched image sizes

Both images use the same Ubuntu Base 26.04.1 ARMhf archive, Docker builder,
APT repositories, cleanup/shrink/compression scripts, web assets, manifest,
FFT archive and cached FPGA/kernel/boot artifacts. The Python image uses the
rootfs package payload and runtime files from `902bd235`; the native image
uses this PR. All **176 common packages have identical versions**. Both images
have the same 32 MiB partition-growth cushion.

| Artifact | Current V1 Python/uWSGI | Native C++23 | Reduction |
|---|---:|---:|---:|
| SD image | 334.00 MiB | 276.00 MiB | **58.00 MiB (17.4%)** |
| Download ZIP | 79.41 MiB | 61.86 MiB | **17.55 MiB (22.1%)** |
| Allocated rootfs content (`du -s -B1`) | 245.44 MiB | 189.46 MiB | **55.98 MiB (22.8%)** |

This comparison isolates this migration, including removal of Python-dependent
cloud-guest-utils and replacement of its growth helper. It does not include
savings from earlier nginx/rootfs PRs. The native image has 179 installed
packages versus 204 for Python. [Raw size evidence](native-runtime-image-size.json)
includes byte counts, SHA-256 hashes, partition/ext4 geometry, all common package
versions and the added/removed packages. Images are built once; ZIP metadata
and filesystem geometry can cause small variations in a rebuild.

## Physical board reboot measurements

Six **software reboots**, in three pairs with alternating pair order:
Python/native, native/Python, Python/native. The board retains the same installed
rootfs and packages for both variants. Kernel, bootloader, FFT 0.3.0 archive,
network configuration and common service ordering are identical. Both variants
use current V1's early-start units; the board's older production ordering is
temporarily replaced in both variants, so earlier PR gains are excluded.
Only the API runtime, nginx upstream protocol and LED runtime differ.

| Median from Linux boot | Current V1 Python/uWSGI | Native C++23 | Change |
|---|---:|---:|---:|
| nginx/API responds successfully | 14.057 s | 10.270 s | **3.787 s earlier (26.9%)** |
| systemd startup finished | 13.483 s | 10.315 s | **3.168 s earlier (23.5%)** |
| `multi-user.target` reached | 10.337 s | 10.267 s | No material change |
| API `ExecStart` to `READY=1` | 4.001 s | 0.556 s | **86.1% shorter** |

An independent [compiled probe](boot_http_probe.cpp) starts after the rootfs
remount, before API startup, and polls nginx on loopback every 10 ms with 50 ms
I/O timeouts. It records the first complete HTTP 200 inventory response.
systemd timestamps independently record service readiness, the target and
completion of all startup jobs. Every boot returns the correct live FFT 0.3.0
and has active instrument, API and nginx services. The Python API becomes ready
after the main OS target, explaining the difference between target readiness and
startup completion. The critical chain still includes SD device/mount startup.

These clocks exclude firmware/bootloader time. They measure reboot readiness,
not power-cycle or LAN/DHCP readiness. The filesystem is already expanded.
The generated Python-free image is **not flashed**: these boots test the native
runtime on the existing rootfs, which still contains Python. Full fresh-image
boot and first-boot growth timing therefore remain unmeasured.

[Raw reboot evidence](native-runtime-boot-red-pitaya.json) includes all six boot
IDs, monotonic timestamps, samples, journals, critical chains, configuration
hashes and executable hashes. Original configuration files and enablement
links are restored and verified against the backup; the production Python API,
kernel, bootloader and instrument archive hashes remain unchanged. The original
API, nginx and FFT are active afterward, with zero failed systemd units. Test
units and the three-minute automatic recovery timer are removed.

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
removed. During this initial isolated test phase, production PIDs remain server
**147**, uWSGI **144**, nginx **150**;
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
boot, its cached FPGA/instrument artifacts, acquisition and ARM64 hardware
tests remain unverified. The subsequent reboot tests above successfully start
the existing board's FFT 0.3.0 with each management runtime.

## Reproduction

The [test README](README.md) gives the Docker command. The board harnesses are
[red_pitaya_benchmark.py](red_pitaya_benchmark.py) and
[red_pitaya_integration.py](red_pitaya_integration.py); their docstrings list
staged inputs and ports. They require an existing Python-based V1 board for the
test harness, not for the native runtime. Run functional checks first, then the
benchmark alone. The reference API is at `902bd235:os/api/__init__.py` and its
adjacent `service_status.py`; obtain those files with `git show`.

The [boot harness](red_pitaya_boot_evidence.py) intentionally changes services
and reboots the test board. Its staged variant trees contain the old/new
nginx configuration and units, with common server/extraction units from current
V1. Python source/LED files, native binaries and missing libraries are staged
under `/var/lib/koheron-native-boot-evidence/bundle`, with unit paths adjusted
to that directory. The standalone probe is built using:

```sh
docker run --rm -v "$PWD:/work" -w /work cross-armhf:26.04 \
  arm-linux-gnueabihf-g++-15 -std=c++23 -O2 -Wall -Wextra \
  os/api/tests/boot_http_probe.cpp -o tmp/boot_http_probe
```

On the prepared board, run `prepare` once, then `select --variant python`
or `select --variant native`, reboot, wait for `/run/koheron-boot-http.json`,
and `collect --output <persistent-path>.json`. Repeat in the order above and
finish with `restore`. Preserve serial/SSH access. The backup and recovery
timer restore the original services if testing loses SSH.

For image sizes, build the Python configured base using `git show 902bd235`
versions of `build_base_rootfs_tar.sh`, `chroot_base_rootfs.sh` and
`finalize_rootfs.sh`, passing the same downloaded Ubuntu Base archive. Copy the
native overlay, replace its API files, LED/growth helpers, uWSGI configuration,
runtime units and nginx site with their `902bd235` versions, and remove the
native API units/binaries. Build it with the baseline `build_image.sh` and
`chroot_overlay.sh`, the same extlinux template, manifest and cached boot/kernel
files. Inspect both images read-only, compare installed package versions and
record `stat -c %s`, SHA-256, `du -s -B1`, `sfdisk --json` and `tune2fs -l`.
