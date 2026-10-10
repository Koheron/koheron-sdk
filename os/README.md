# OS image builds

Build and installation: [SDK quick start](../README.md#quick-start).

## Settings

The default runtime is Ubuntu Base **26.04.1** and Xilinx **2026.1 / Linux 6.18**, independent of `VIVADO_VERSION` (default **2025.1**). Bootloader, firmware and device-tree source releases follow the selected Vivado/Vitis toolchain. The reference development host remains Ubuntu 24.04; the default build container uses Ubuntu 26.04 and GCC 15.

Override `LINUX_VERSION` to select an earlier kernel release, for example:

```sh
make CFG=examples/alpha250/fft/config.mk VIVADO_VERSION=2025.1 \
  LINUX_VERSION=2025.1 image
```

The native management runtime and its shared libraries target Ubuntu 26.04.
Older rootfs releases require a matching custom builder and package selection.

APT repositories are derived from the extracted Ubuntu rootfs's `/etc/os-release`. New Xilinx source releases require entries in `source-checksums.sha256`. Defaults refer to released versions, not development snapshots or moving branches.

Rebuild boot and FPGA artifacts when changing Vivado/Vitis versions; cached artifacts are not proof of compatibility. Validate image builds separately from board boot, FPGA overlays, DMA/cache correctness and acquisition tests.

Zynq images set `RebootWatchdogSec=8min` through a systemd manager drop-in. The
Cadence watchdog driver's 516-second maximum rejects systemd's 10-minute default.
This timeout covers the final reboot phase, after regular services stop;
individual service stop timeouts continue to control their cleanup. Other
platforms retain their existing watchdog settings.

The SDK defconfigs include the full Xilinx 2025.1 → 2026.1 config delta, compared against upstream [`xilinx_zynq_defconfig`](https://github.com/Xilinx/linux-xlnx/blob/xilinx-v2026.1/arch/arm/configs/xilinx_zynq_defconfig) and [`xilinx_defconfig`](https://github.com/Xilinx/linux-xlnx/blob/xilinx-v2026.1/arch/arm64/configs/xilinx_defconfig). Zynq gains explicit ext4, ACL and security-label support and removes redundant scheduler selections. ARM64 gains thermal support, CoreSight/default tracing, NVMe, Type-C, I3C, GPIO aggregation, RPMsg TTY and additional PCIe/PHY support; INA power monitors move from IIO to hwmon, and obsolete selections are removed. Koheron-specific settings are retained. Kconfig dependencies determine which drivers apply to each platform; these selections do not change the Vivado version or runtime kernel default.

Set `PASSWORD` and `TIMEZONE` in the environment before `make image` to override the defaults in [rootfs.mk](./rootfs.mk).

The image manifest records source tags separately from `vivado`, `vivado_build`,
`vitis` and `vitis_build`, queried from `VIVADO_PATH` and `VITIS_PATH` at packaging.
Unavailable values produce a warning and `unknown`. This identifies the selected
tools, not the history of cached artifacts; rebuild artifacts after changing tools.

The image uses glibc's built-in `C.UTF-8` locale. UTF-8 text remains supported,
with C sorting and formatting conventions. To use another locale, install
`locales`, generate the desired locale and run `update-locale`. Image finalization
removes APT caches and package documentation, including files already present in
Ubuntu Base, while retaining copyright notices and runtime encoding data.

The C++23 management API starts eagerly alongside other services and inherits
its HTTP Unix socket from systemd. It reports readiness only after the inventory
and listener are usable. Its startup does not gate `basic.target`.
Instrument extraction, the server and nginx also start with their existing early
boot prerequisites, without blocking `basic.target` or services such as SSH.
The server still waits for extraction and reports readiness with `Type=notify`;
the IP LED helper waits for that readiness. These early services explicitly stop
before `shutdown.target`. Image assembly recreates their enablement links under
`multi-user.target`, removing legacy links under `basic.target`.
The IP LED helper waits for an IPv4 address on `end0` or legacy `eth0` in the
background, including when DHCP arrives after boot. It does not pull in
`network-online.target`. A completed boot does not imply that DHCP or the web API
is already ready; measure those separately. The helper does not monitor later
address changes after displaying the IP.

## Runtime FPGA loading

Runtime instrument overlays describe devices only. The server removes the previous
overlay, programs the full bitstream through the Xilinx FPGA Manager `firmware`
attribute, verifies its `operating` state, then applies `pl.dtbo`. The generated
overlay omits `firmware-name` and exported `__symbols__`, while retaining external
and local phandle fixups. This avoids retaining properties on permanent
device-tree nodes each time an instrument is switched. Boot-time board overlays
still export symbols.

Rebuild the server and `pl.dtbo` together when updating an existing instrument;
the bitstream itself does not need rebuilding for this loading change. The direct
loading path requires Xilinx's FPGA Manager sysfs interface (available in the
2025.1 and 2026.1 kernels) and supports full-device designs without FPGA bridges.
Bridge-dependent custom designs are rejected before removing the current overlay.
Partial reconfiguration and stacked runtime overlays are not supported by this
path. The legacy `/dev/xdevcfg` path is unchanged.

The loader checks both the configfs overlay path and its status: Xilinx configfs
can report `applied` after rejecting a malformed DTBO, but clears its path on
failure. A failed load exits before driver initialization and lets the instrument
installer restore and reprogram the previous installation.

## Tests

Image-build tests are in [tests/](./tests/); instrument loading tests are in [api/tests/](./api/tests/).

## Runtime kernel features

The Zynq and ZynqMP defconfigs build in Unix socket diagnostics, autofs,
UTS/network namespaces, cgroup BPF and nftables for
systemd services, and SysRq/Yama for the distribution's sysctl settings. These
features are built in because the OS image does not install kernel modules.
The kernel log buffer is 128 KiB to retain boot diagnostics before journald starts.

After changing either defconfig, check the generated kernel `.config`, rebuild
the OS image, and verify boot logs, SSH, the management API and instrument
switching on the target board. A kernel rebuild does not require an FPGA rebuild.

## Management web interface

The board runtime consists of three C++23 executables in `/usr/local/api`:
`koheron-api`, `koheron-install` and `koheron-server-init`. nginx proxies HTTP to
the systemd-managed `/run/koheron-api/app.sock`. The daemon uses libmicrohttpd,
libzip, json-c, libunistring and libsystemd; it queries service state and journals directly.
The existing 16 HTTP routes, upload field naming and host SDK remain compatible.
Instrument installation stages and validates files before stopping the service,
waits for systemd readiness and restores the previous installation on failure.
Uploads are capped at 20 MiB; extraction is capped at 256 MiB and 10,000 entries.

The management API also exposes deployment checks, service controls, board
health and diagnostic export. The existing host SDK routes retain their response
formats. New mutations require POST; their failures return JSON with `code`,
`error` and `rollback` (`not_needed`, `restored` or `failed`). An operation in
progress rejects another new mutation with HTTP 409 instead of queuing it.

| Method | Route | Result |
| --- | --- | --- |
| GET | `/api/instruments/preflight/NAME` | Compatibility, archive contents/size, staging capacity, warnings and `ready`. |
| POST | `/api/instruments/activate/NAME` | Validate, stage and activate; return the final operation result. |
| POST | `/api/instruments/control/start` | Start the loaded installation without re-extracting its ZIP. |
| POST | `/api/instruments/control/stop` | Stop it and retain its files and identity. |
| POST | `/api/instruments/control/restart` | Restart the loaded installation without re-extraction. |
| POST | `/api/instruments/default/NAME` | Validate and persist the boot preference without changing the running instrument. |
| GET | `/api/system/status` | Instrument inventory, loaded identity, current/last operation and sampled health. |
| GET | `/api/system/diagnostics` | Download metadata, status and bounded journal excerpts as JSON. |
| WebSocket | `/api/events` | The same status snapshots, on operation changes and every two seconds. |
| WebSocket | `/api/logs/koheron/events?cursor=CURSOR` | Recent instrument logs followed by incremental journal batches. Cursor is optional. |
| GET | `/api/logs/koheron/tail?cursor=CURSOR` | One bounded batch using the same log protocol, for HTTP fallback. |

Service controls use systemd D-Bus jobs and wait for completion, including
`Type=notify` readiness. Activation reports validation, extraction, stopping,
starting and rollback. The loaded identity remains available when the service
is stopped. Legacy archives remain usable, with a warning that board compatibility
cannot be verified. New builds include `instrument.json` with format version 1,
board, architecture, SDK version and minimum management API version. ELF machine
and class checks also reject a mismatched executable independently of metadata.
Checks run before stopping an instrument; they do not prove that an FPGA design
or driver will work on hardware.

Preflight budgets the new extraction, file allocation overhead and a 4 MiB
reserve while the previous files remain allocated. External processes can still
consume space after the check; extraction failures leave the running instrument
intact. Uploads have a shared two-writer limit and free-space reservations.
nginx streams request bodies directly to the native writer. File contents and
containing directories are synced for archive and boot preference commits. If
directory syncing fails after a rename, the API reports failure and keeps its
inventory consistent with the visible files; callers should read status before
retrying.

Browser API requests must have matching Origin/Referer and fetch-site headers
when present. Navigation and embedded-resource requests cannot run mutations,
and HEAD never activates or removes an instrument. nginx preserves the request
host, including its port, and supplies the actual scheme. SDK and CLI clients
without browser provenance headers retain the existing GET run/delete routes.
Legacy text replies use `text/plain` with `nosniff`; JSON contracts are unchanged.
Compatibility JSON must be valid UTF-8, strict JSON, and contain no trailing data.
Replacing the selected boot-default archive reruns preflight before committing
the upload. Invalid replacements preserve the existing archive and preference.

Health reads Linux uptime, load averages, `MemAvailable`, filesystem capacity
and systemd service properties. Samples are shared for two seconds. Timing
values use microseconds: API initialization covers inventory/listener setup,
while service timestamps refer to the current invocation on the monotonic boot
clock. Server startup is readiness minus execution start; boot extraction is
the extraction service's execution duration. Restarting the API or instrument
updates its invocation timing; these values are not a fresh cold-boot benchmark.
Unavailable values stay unavailable rather than becoming zero readings.
Diagnostic exports limit journal messages to 4 KiB each and 512 KiB of encoded
journal context. Oversized inventories or build metadata are omitted explicitly;
the complete download is bounded to 1 MiB.

Each WebSocket endpoint is read-only and accepts at most eight clients, leaving HTTP
capacity for commands. It checks browser origin, handles ping/close frames and
disconnects slow clients with bounded buffering. Every ten seconds it sends a
ping; peers must return the matching pong within five seconds to retain their
slot. The management page shares one
connection for instrument state, activation progress and health. It reconnects,
falls back to HTTP status reads, and suspends the connection while hidden.
Logs have a separate connection so Pause releases the journal reader without
interrupting controls or health. The native reader stays open and follows
journal notifications; the browser no longer polls while that stream is healthy.
Both transports return `{type: "logs", cursor, entries, reset}`; entries contain
microsecond `ts`, `msg` and systemd `prio` (0–7). Initial history is the latest
200 entries. Each batch has at most 200 entries, 4 KiB of source message per
entry and less than 64 KiB of encoded JSON; oversized messages are truncated.
Backlogs drain in bounded batches. Empty batches every two seconds keep the
browser watchdog alive. Reconnect and Resume seek after the last delivered
cursor. An invalid or expired cursor returns `reset: true` and recent history;
the widget clears its old history to avoid duplicates. The display retains
1,000 grouped rows, follow and download controls.

Blocked or stalled log sockets retry with a 1–10 second backoff and use bounded
HTTP batches once per second in the meantime; the badge reads “Live · polling”.
HTTP log reads time out after eight seconds. Pause, hidden pages and page
transitions cancel pending reads and close the log socket; BFCache restoration
resumes it. Late replies cannot overwrite a newer cursor or update paused logs.
For log peers with blocked writes, the API stops reading and closes them after
two seconds; outgoing buffers remain bounded. The original journal HTTP routes
remain available for SDK clients.
HTTP status reads time out after eight seconds and are cancelled when the page
is hidden or disposed. Late HTTP results cannot replace a newer WebSocket
snapshot. Lost or malformed status disables mutations until valid status returns.

Boot extraction uses `koheron-install --extract-default`. It reads the selected
archive from `/usr/local/instruments/default`, validates and stages it before
replacing `/tmp/live-instrument`, and does not control services. systemd starts
the server after extraction succeeds. The existing
`unzip-default-instrument.service` name is retained for compatibility.

Loader detection follows the server: `/dev/xdevcfg` takes precedence over FPGA
Manager. FPGA Manager extraction omits reference `.bit` files while retaining
`.bit.bin` and `pl.dtbo`; xdevcfg retains `.bit`. Omitted files still undergo CRC
and size validation. Without board devices, offline `auto` extraction retains
both payload formats. For explicit offline staging:

```sh
koheron-install --extract-default --instruments /path/to/instruments \
  --live /path/to/staging/live --loader overlay
```

This mode only extracts files; use normal installation or the API to switch a
running instrument. Invalid input leaves the previous extraction intact, and a
failed file swap restores it. If restoration fails, the installer reports and
retains the backup path.

The standard image has no Python interpreter, Flask, uWSGI, cloud-guest-utils
or unzip.
First-boot partition growth uses `sfdisk`, followed by `partx` and `resize2fs`.
Host Python clients and build/test tools are unchanged.

`make CFG=... api` builds the native executables in Docker. `api_sync` updates
the binaries, units and nginx configuration; it checks shared-library loading
before switching services. For an older Ubuntu 26.04 V1 image, install
`libmicrohttpd12t64 libzip5 libjson-c5 libunistring5 libstdc++6` first. Rebuild the full image
to remove the old Python packages. Ubuntu 24.04 board images require a matching
custom toolchain and dependencies.

nginx uses two `www-data` workers, low-cost gzip compression for HTML,
and bounded API response buffers. Upload buffering uses
`/run/nginx` to avoid SD-card writes; the nginx unit creates that directory at
boot. WebSocket replies remain unbuffered. Open-file caching is left disabled
so replacing an instrument takes effect immediately. Instrument web files must
be readable, and their parent directories traversable, by `www-data`.
JavaScript and CSS are served without dynamic compression: Red Pitaya LAN tests
showed that per-request gzip reduced bytes but increased download latency.

The OS serves the management pages at `/koheron/`: installed instruments,
running status, server logs, system information and data rates. These pages
live in `os/www/` and share the instrument control styles in
`web/instrument/instrument.css`. Their assets ship with the OS image. The pages
use content versions for their script and stylesheet URLs, and nginx requires
cache revalidation. This keeps existing browser profiles on the current assets
after an in-place API/UI update. `make www` refreshes the three management pages
when their script or stylesheet contents change.
The single-page manager shows installed instruments, logs and system details together. It supports
upload/run/remove feedback, live status updates and log pause, follow and download.
The live instrument strip adds Start/Stop/Restart. Each instrument's More menu
offers compatibility checks, boot selection and removal; the details page also
shows preflight results. Health and diagnostic download use the existing sidebar,
with build metadata collapsed underneath. Activation progress and rollback
outcomes share one status line above the instrument list.

Build them with `make CFG=examples/alpha250/fft/config.mk www`. The output is
`tmp/www/`. Run the host regression suite after installing the dependencies
from `web/package.json`:

```sh
NODE_PATH=web/node_modules node --test os/www/tests/test_*.cjs
```

The suite uses simulated HTTP responses and does not control hardware. Verify
upload, run, removal and log streaming on a board before deploying an OS image.

The data-rate monitor converts server rates from bits/s to bytes/s, uses server
timestamps for its four-minute history, and marks unchanged snapshots as stale.
Monitor tests cover scaling, unit conversions, polling lifecycle and stale data.
