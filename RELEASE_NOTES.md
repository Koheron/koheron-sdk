# Koheron SDK V1 changelog

V1 is under development. This is a functional changelog of the SDK as of
13 October 2026, comparing these revisions:

- V0: [`master` at `d37dd080`](https://github.com/Koheron/koheron-sdk/tree/d37dd08061d0fc7e63d04b327c1fc530e054186c).
- V1: [`V1` at `ab2c6186`](https://github.com/Koheron/koheron-sdk/tree/ab2c618648407386f9da21961cd98bf3a5a6724c).

Related commits are grouped by their resulting behavior. This covers build
tools, shared libraries, board support and the Linux runtime; changes to
individual instruments are excluded. Some work also reached V0 after the
V0.24 tag; see [changes already in V0](#changes-already-in-v0) before treating
an item in the Git history as a V1-only change.

Boards still ship with V0. Use a V1 OS image for V1 instruments; a V1 image
cannot run a V0 instrument. The [migration guide](MIGRATING.md) gives the
configuration and C++ edits needed for a minimal port.

## Contents

- [Migration requirements](#migration-requirements)
- [Build configuration and developer commands](#build-configuration-and-developer-commands)
- [Toolchains and defaults](#toolchains-and-defaults)
- [FPGA builds and Linux integration](#fpga-builds-and-linux-integration)
- [Memory, DMA and interrupts](#memory-dma-and-interrupts)
- [C++ drivers and server runtime](#c-drivers-and-server-runtime)
- [Client libraries and shared web code](#client-libraries-and-shared-web-code)
- [OS images and boot](#os-images-and-boot)
- [Board management and deployment](#board-management-and-deployment)
- [Board support and shared hardware drivers](#board-support-and-shared-hardware-drivers)
- [Tests and validation](#tests-and-validation)
- [Changes already in V0](#changes-already-in-v0)

## Migration requirements

| Area | V0 | V1 / action for a custom project |
| --- | --- | --- |
| Project selection | `CONFIG=.../config.yml`; a default project could be selected implicitly. | Build commands require `CFG=.../config.mk`. |
| Configuration | Build inputs and memory definitions in `config.yml`. | Put build inputs in `config.mk` and memory definitions in `memory.yml`. |
| Instrument C++ | C++17, GCC 9 by default. | C++23, GCC 15 by default; rebuild custom code and dependencies. |
| Driver dependencies | Usually supplied through `Context&`. | Default constructors and service access are preferred. `Context&` remains supported but deprecated. |
| Memory API | `Memory<...>` and context memory access. | Use `hw::Memory<...>` and the hardware service headers. |
| FPGA loading | `/dev/xdevcfg`. | FPGA Manager and device-tree overlays on a V1 OS image. |
| Reserved DMA memory | Custom designs could access fixed physical addresses directly. | Reserve fixed DMA buffers in the boot device tree; V1 rejects configured `mem_wc` regions that overlap Linux System RAM. |
| Board management | Python/uWSGI application. | Native C++ service; install Python dependencies separately if custom board scripts need them. |
| SDK extensions | Server internals concentrated under the old core/context layout. | Port extensions to the `network`, `executor`, `runtime` and `hardware` interfaces described below. |

## Build configuration and developer commands

### Configuration and generated files

- Replaced YAML build settings with Make variables. `config.mk` defines
  `NAME`, `VERSION`, `BOARD_PATH`, `CORES`, `XDC`, `DRIVERS` and `WEB_FILES`.
  Projects can use Make includes, conditionals and command-line overrides
  instead of extending the YAML schema for each build option.
- Split the hardware/software memory description into `memory.yml`. Registers
  belong to their memory region rather than separate top-level control/status
  lists. The same description generates `memory.hpp`, `memory.tcl` and
  `memory.dtsi` so C++, Vivado and Linux use matching definitions.
- Added memory-region `dev` and `compatible` settings for Linux mappings and
  device-tree bindings. Project `override.dtsi` files can add or override
  device-tree properties.
- Added configuration validation for malformed regions, registers and project
  inputs. Memory overlaps are reported as warnings rather than universally
  rejected, since some designs intentionally use aliases.
- Added support for driver implementations in `.cpp` files alongside driver
  headers. Declare the implementation sources in `DRIVERS` when splitting
  custom drivers across translation units.
- Project Makefiles use `SDK_PATH` and `PROJECT_PATH` for SDK and project includes.
  Equivalent in-tree CFG paths resolve to the same cache. External projects use
  `tmp/.external/<path-hash>` under the selected `TMP` directory, avoiding cache
  collisions between projects with the same directory name.

See [configuration generation](make.py), the
[configuration split](https://github.com/Koheron/koheron-sdk/commit/e32c4e84) and
[per-region registers (#645)](https://github.com/Koheron/koheron-sdk/pull/645).

### Inspection, diagnostics and board commands

The command interface now supports inspection and automation without running
the entire build:

| Command | Added or changed behavior |
| --- | --- |
| `make help`, `make list` | Discover targets and available example configurations. |
| `make info CFG=...` | Resolve inputs, toolchains and artifact paths without building. |
| `make doctor TARGET=... [CFG=...]` | Check requirements for the requested task; web/server checks do not require Xilinx tools. |
| `make validate CFG=...` | Validate build and memory configuration. |
| `make check [CFG=...]` | Validate configurations and run local host regressions without synthesis or board access. |
| `make copy CFG=... DEST=...` | Copy a project's sources and set `NAME` from the destination, preserving shared references. It does not rename C++ classes or client API names. |
| `make timing CFG=...` | Check the routed FPGA design. |
| `make deploy CFG=... HOST=...` | Build, upload, preflight and activate; return after confirmed readiness. |
| `make run CFG=... HOST=...` | Build and start the instrument, then follow logs. Ctrl+C leaves it running. |
| `make status HOST=...`, `make logs HOST=...` | Inspect the board without CFG or build tools. |
| `make test CFG=... HOST=...` | Run the project's hardware test script; `INSTRUMENT_TEST` can select another script. |
| `make flash CFG=...` | Flash an existing release ZIP; image building is a separate operation. |

`info`, `doctor`, `check`, `deploy`, `status` and `logs` support `FORMAT=json`
with `schema_version: 1`. Diagnostics and board commands report `ok` and return
nonzero on failure. Deployment build output goes to stderr so stdout remains
machine-readable. `HOST_PYTHON` selects a prepared interpreter for these host
tools.

See the [development guide](docs/development.md) and
[developer workflow changes (#857)](https://github.com/Koheron/koheron-sdk/pull/857).

### Incremental builds and packaging

- Added compiler caching, precompiled headers and more selective generated
  header dependencies. Driver object paths preserve source directories so
  implementation files with equal basenames do not collide.
- Made build signatures track compiler options, tools, source membership,
  driver order, memory definitions and FPGA/board inputs. Switching settings
  back to an earlier value must also invalidate incompatible outputs.
- Shared expensive image inputs across instruments: Linux builds by
  architecture, U-Boot builds by board, and base root filesystems by their
  architecture and settings. Their dependencies include the selected releases
  and relevant configuration/scripts.
- Fixed missing FPGA Tcl/constraint dependencies and web build dependencies.
  Changes to shared inputs now trigger the affected stages.
- Build instrument archives from a fresh staging area and publish them
  atomically. Removed web files no longer remain in an updated ZIP, and a
  failed packaging step does not replace the last successful archive.
- Added `instrument.json` with SDK version, board, architecture,
  minimum runtime requirements and SDK commit. Packages also carry
  `drivers.json` for API inspection, alongside `serverd`, FPGA files and web
  assets. The instrument version remains in the separate `version` file.
- Moved SDK commit provenance into package metadata. A commit-only change
  updates provenance without forcing a server recompile.

See [instrument packaging](make/instrument.mk),
[build caching (#855)](https://github.com/Koheron/koheron-sdk/pull/855),
[cache invalidation (#856)](https://github.com/Koheron/koheron-sdk/pull/856),
[dependency fixes (#865)](https://github.com/Koheron/koheron-sdk/pull/865) and
[package provenance (#860)](https://github.com/Koheron/koheron-sdk/pull/860).

## Toolchains and defaults

These are defaults at the compared revisions, not a requirement to tie every
component to the same Xilinx release.

| Component | V0 default | V1 default |
| --- | --- | --- |
| Instrument server language | C++17 | C++23 |
| Server compiler | GCC 9 | GCC 15 |
| Vivado/Vitis selection | Vivado 2017.2 | Vivado/Vitis 2025.1 |
| Docker compiler image | `gnu-gcc-9.5` | `cross-armhf:26.04` |
| Ubuntu Base for image builds | 22.04.4 | 26.04.1 |
| Linux and U-Boot selection | Koheron forks, selected using the Vivado release | Xilinx 2026.1 sources, with independent version settings |
| Web build | Host npm/TypeScript tools | Node 24 Docker builder and locked npm dependencies |

- The reference host is Ubuntu 24.04. `make setup` installs the other build
  dependencies and builder images; install Vivado/Vitis separately.
- The default compiler image supplies native, ARM hard-float and AArch64 GCC
  15 toolchains. Server, kernel, U-Boot and ARM Trusted Firmware compiler
  versions can be selected independently with `HOST_GCC_VERSION`,
  `KERNEL_GCC_VERSION`, `UBOOT_GCC_VERSION` and `ATF_GCC_VERSION`, provided the
  selected `DOCKER_IMAGE` contains them. FSBL/PMU firmware uses the bare-metal
  toolchains.
- Cross sysroot headers and libraries match the default V1 Ubuntu 26.04
  rootfs. Custom runtime images must supply compatible libraries.
- The standalone C++ client supports C++20; the server's C++23 requirement
  does not apply to it.
- Boot firmware generation uses `.xsa` hardware handoffs and selects `xsdb`
  for Vitis 2024 and later, retaining the older `xsct` path for earlier tools.

See [compiler settings](docker/README.md),
[toolchain updates (#804)](https://github.com/Koheron/koheron-sdk/pull/804),
[C++23 (#807)](https://github.com/Koheron/koheron-sdk/pull/807) and
[xsdb support (#663)](https://github.com/Koheron/koheron-sdk/pull/663).

## FPGA builds and Linux integration

- Added FPGA Manager output to instrument builds: `<instrument>.bit.bin`
  and a compiled `pl.dtbo` device-tree overlay. The original `.bit` remains in
  the package for reference.
- Generate the overlay from the programmable-logic design, `memory.yml` and
  optional `override.dtsi`. Linux devices can now be bound to instrument PL
  peripherals when an instrument is loaded. Base boot device-tree settings
  remain separate from these runtime overlays.
- Load the firmware before applying the runtime overlay so Linux drivers do
  not probe unprogrammed hardware. The runtime uses the live firmware search
  path and handles overlay replacement rather than accumulating overlays.
- Retained a legacy `/dev/xdevcfg` fallback in the loader. It does not provide
  the complete V1 Linux device/runtime environment; use a V1 image for V1
  projects.
- Added routed timing checks covering setup, hold, pulse width and bus skew.
  `ENFORCE_TIMING=1` makes failures fail the build; enforcement is opt-in at
  SDK level and can be enabled by a project. Passing these checks does not
  establish that all external I/O paths are constrained.
- Added optional per-IP synthesis caching with `FPGA_SYNTH_MODE=ip`; the
  default remains global synthesis. The cache is separated by Vivado version.
- Added a shared IP packaging framework with optional `package_ip.tcl`, GUI
  customization, vendor XCI inputs and export ZIPs. FPGA dependencies include
  project `TCL_FILES` as well as cores and constraints.
- Hardened the shared AXI-Lite control/status interfaces: independent write
  address/data handshakes, byte strobes and stalled responses are handled
  explicitly. The control interface offers optional write predecoding; the
  status interface pipelines reads. Custom IP using these interfaces should
  follow their handshake and latency contracts.

See [FPGA build settings](fpga/README.md), [IP packaging](fpga/ip/README.md),
[overlay support (#646)](https://github.com/Koheron/koheron-sdk/pull/646),
[firmware load ordering (#816)](https://github.com/Koheron/koheron-sdk/pull/816),
[shared synthesis cache (#833)](https://github.com/Koheron/koheron-sdk/pull/833)
and [AXI status pipelining (#868)](https://github.com/Koheron/koheron-sdk/pull/868).

## Memory, DMA and interrupts

- Added the `mem_wc` Linux driver and device-tree mappings for write-combining
  access to selected physical memory regions. The C++ memory layer chooses
  the configured device; ordinary regions can still use `/dev/mem`.
- Added UIO mapping by physical address, timed interrupt waits and asynchronous
  callbacks. The shared FIFO driver can use interrupts instead of only polling.
  Cancellation and destruction stop asynchronous waits and release their
  resources. Falling back to `/dev/mem` does not supply UIO interrupts.
- Added checked waits to the shared simple-mode DMA driver: report transfer
  errors/timeouts and refuse operations that would reset an active
  scatter/gather engine.
- Reserved the fixed 128 MiB DMA window at `0x18000000` in the boot device-tree
  configuration for ALPHA250-family and Red Pitaya boards. These buffers use
  `reserved-memory` with `no-map`, keeping them outside normal Linux RAM.
- Added a `/proc/iomem` guard for regions configured with `/dev/mem_wc*`,
  including when they fall back to `/dev/mem`. It rejects overlap with Linux
  System RAM. A runtime overlay cannot retroactively make an old image's
  System RAM safe for fixed-address DMA; rebuild the boot image with the
  reservation.
- Brought the CMA allocator into the SDK and fixed buffer ownership. Separate
  opens own separate, zeroed buffers; duplicated descriptors share their
  allocation. A live mapping retains the allocation after the descriptor is
  closed. Reallocation while mapped returns `EBUSY`, and a failed replacement
  allocation preserves the old buffer. The existing 32-bit ioctl ABI is
  retained.
- CMA allocation does not stop an FPGA DMA engine or provide device-specific
  cache synchronization. Custom drivers still manage transfer completion and
  CPU/device ownership.
- Hardened memory-map initialization, retry/cleanup and range checks. Span
  bounds include the requested offset. Borrowed `read_span` results remain
  tied to the lifetime of the mapping.

See the [CMA contract](os/patches/linux/drivers/koheron/README.md),
[write-combining memory (#643)](https://github.com/Koheron/koheron-sdk/pull/643),
[DMA support (#644)](https://github.com/Koheron/koheron-sdk/pull/644),
[CMA ownership (#708)](https://github.com/Koheron/koheron-sdk/pull/708) and
[fixed DMA reservations (#726)](https://github.com/Koheron/koheron-sdk/pull/726).

## C++ drivers and server runtime

### Driver construction and service access

The runtime now provides shared services independently of the legacy `Context`
object. A driver can have a default constructor and request the services it
needs directly. Existing constructors taking `Context&` continue to work
through a deprecated compatibility layer.

| V0 pattern | V1 equivalent |
| --- | --- |
| `Memory<...>` | `hw::Memory<...>` from `"server/hardware/memory_map.hpp"`. |
| `ctx.mm.get<...>()` | `hw::get_memory<...>()` from `"server/hardware/memory_manager.hpp"`. |
| `ctx.get<Driver>()` | `rt::get_driver<Driver>()` from `"server/runtime/driver_manager.hpp"`. |
| `ctx.spi`, `ctx.i2c` | `services::require<hw::SpiManager>()` / `services::require<hw::I2cManager>()`, from `"server/runtime/services.hpp"` with `"server/hardware/spi_manager.hpp"` / `"server/hardware/i2c_manager.hpp"`. |
| `ctx.log<...>(...)` | Free logging functions from `"server/runtime/syslog.hpp"`; use `logf` for format-string logging. |

- Split server responsibilities into `network` (sessions/transports),
  `executor` (RPC dispatch), `runtime` (services, drivers and configuration)
  and `hardware` (FPGA, buses and memory). Custom code including old internal
  headers needs to use the corresponding new interfaces.
- Added typed service registration/access with `provide`, `get` and `require`.
  Prefer these APIs in new drivers rather than making a new `Context`
  dependency.
- Made initialization order explicit: FPGA and hardware services are ready
  before driver initialization and RPC listener startup. Custom startup hooks
  should be ported with that ordering in mind.
- Added an INI configuration manager for
  `/etc/koheron/<instrument>/config.ini`, including load/save and file-creation
  error reporting. Individual drivers decide which settings to persist.
- Reworked generated driver tables and member-function adapters for C++ RPC
  dispatch. Python still parses driver declarations and generates metadata;
  adding or changing exported methods requires regenerating and rebuilding.
- Added `IExecutor` for custom dispatch integration and `server/serverless.mk`
  with `SERVERLESS_CPP_SRCS` for applications using SDK runtime/hardware code
  with their own entry point.
- Fixed shutdown ordering: join session workers and stop drivers before
  destroying the services and mappings they can access.

See the [C++ migration steps](MIGRATING.md), [runtime code](server/runtime),
[hardware services](server/hardware), [serverless build](server/serverless.mk)
and [shutdown ordering (#821)](https://github.com/Koheron/koheron-sdk/pull/821).

### RPC behavior, correctness and throughput

- Retained TCP/WebSocket RPC and method discovery. Clients should resolve
  driver/method names through discovery rather than hard-code generated IDs.
- Reject invalid driver IDs and oversized variable-length arguments before
  unsafe dispatch or allocation.
- Corrected raw array byte-order handling. Arrays carry their byte sequence
  rather than applying scalar argument conversion to each byte block.
- Batch fixed-size argument reception before acquiring the driver lock.
  Owned replies can release that lock before sending; borrowed memory replies
  remain protected for the duration of the send.
- Reduced allocation/copying for small replies and mixed scalar/buffer replies.
  Added scatter/gather WebSocket sends and ARM NEON WebSocket unmasking.
- Fixed short I/O, WebSocket EOF/header handling, disconnect cleanup and
  session lifetime problems, including destruction while a worker is active.
- Disabled `MSG_ZEROCOPY` by default because safe use requires tracking
  asynchronous buffer completion; ordinary sends retain synchronous buffer
  lifetime guarantees.
- Added format-string logging and skip formatting for disabled log levels.

See [array serialization (#687)](https://github.com/Koheron/koheron-sdk/pull/687),
[zero-copy safety (#688)](https://github.com/Koheron/koheron-sdk/pull/688),
[argument batching (#758)](https://github.com/Koheron/koheron-sdk/pull/758),
[WebSocket sends (#776)](https://github.com/Koheron/koheron-sdk/pull/776),
[session lifetime (#805)](https://github.com/Koheron/koheron-sdk/pull/805) and
[driver-ID checks (#806)](https://github.com/Koheron/koheron-sdk/pull/806).

## Client libraries and shared web code

- Python transport uses complete sends and preallocated receive buffers, with
  explicit handling of closed connections. Type handling includes long-long
  integer declarations and 64-bit unsigned aliases.
- Python upload/run helpers check HTTP error responses. Uploads use the archive
  basename consistently. Calling `run_instrument` without a name when nothing
  is running now reports an explicit error.
- Added Python log bookmark/stream helpers and instrument log helpers, with
  cursor-based WebSocket streaming and HTTP fallback.
- Hardened the standalone C++ client: own hostname storage, use the Windows
  invalid-socket value correctly, and check class discovery and runtime
  argument types. The basic C++ HTTP management API also exists in current
  V0; it is not a V1-only addition.
- TypeScript initialization and read methods support Promises as well as the
  callback interface. Initialization has a timeout and failed initialization
  closes the socket pool. Reads validate frame sizes and reject failures.
  Clients can install a disconnect handler instead of always reloading the page.
- Consolidated shared browser components and their build dependencies under
  `web/`. The web build uses a dedicated Node 24 image and locked dependencies
  (TypeScript 7.0.2 at this revision).
- Vendored the shared Flot plotting code in the repository,
  including its licenses. Canvas rendering and draw-time reductions improve
  shared plotting performance while retaining underlying data for exports.

See [Python client code](python/koheron),
[HTTP failure handling (#696)](https://github.com/Koheron/koheron-sdk/pull/696),
[C++ client](server/client), [TypeScript client](web/koheron.ts) and
[shared plotting code](web/plotting/README.md).

## OS images and boot

- Split rootfs construction into a reusable base and board/project overlays.
  Cache inputs include architecture, Ubuntu version, password/timezone
  settings and build scripts. APT sources follow the selected rootfs release.
- Added `./build.sh <board>` presets for building images with the board's
  selected instruments and boot default. `COPY_INSTRUMENTS` controls additional
  packaged instruments; direct `make image CFG=...` remains available.
- Changed boot packaging from the old `uImage`/`uEnv.txt` layout to FIT
  `image.ub` and extlinux configuration. Boot firmware generation handles
  board-specific components; ZynqMP includes PMU firmware and ARM Trusted
  Firmware with explicit A53 toolchain selection.
- Use Xilinx Linux/U-Boot release sources with board patches and independent
  version selection. Device-tree compiler generation no longer needs a full
  kernel build solely to obtain `dtc`.
- Use `systemd-networkd` and `systemd-resolved`, with DHCP configurations for
  `end0`/`end1`. Custom `/etc/network/interfaces` changes need to be ported to
  the new network configuration.
- Replaced the board's Python/uWSGI management stack with a native service and
  removed its Python runtime dependencies from the default image. Host-side
  build tools and Python clients still use Python. Custom Python programs on
  the board must arrange their own packages.
- Hardened first boot: root partition expansion can retry after failure,
  existing SSH host keys are preserved, and key generation completes before
  SSH starts. Service ordering coordinates image initialization and instrument
  startup.
- Added image manifests and `/etc/koheron-release` information recording the
  actual selected versions. Downloads are verified and published atomically.
- Fail image construction on APT, chroot, mount, overlay and service-enable
  errors rather than publishing a partial result. Check partition bounds
  before truncation and release loop devices/mounts before publishing.
- Reduced image size by removing build helpers and clearing unused space.
  Image reuse, shrinking and flashing checks protect existing artifacts from
  incomplete or invalid rebuilds.

See [OS settings and layout](os/README.md), [rootfs build](os/rootfs.mk),
[rootfs configuration (#683)](https://github.com/Koheron/koheron-sdk/pull/683),
[image safety (#746)](https://github.com/Koheron/koheron-sdk/pull/746),
[native management (#832)](https://github.com/Koheron/koheron-sdk/pull/832),
[boot archive validation (#835)](https://github.com/Koheron/koheron-sdk/pull/835)
and [U-Boot update (#854)](https://github.com/Koheron/koheron-sdk/pull/854).

## Board management and deployment

### Installation and compatibility checks

- The C++23 management service preserves the existing host SDK routes and adds
  explicit preflight, activation, service-control, status and log APIs.
- Preflight checks archive structure, identity, board, architecture and runtime
  requirements from `instrument.json`. Archives without the new metadata can
  be accepted with a warning; this does not make a V0 binary compatible with
  a V1 OS image.
- Uploads and activation use staging. An interrupted/rejected upload preserves
  the existing archive. Activation validates the payload, waits for service
  readiness and attempts to restore the previous installation if it fails.
- New mutation endpoints use POST and serialize operations; a concurrent
  mutation receives HTTP 409. Failure responses include `code`, `error` and a
  `rollback` result. Uploads are limited to 20 MiB.
- The management web interface exposes installed/loaded state, activation and
  boot-default controls, live logs, system/build information and driver API
  metadata.

### API additions

| Method | Route | Purpose |
| --- | --- | --- |
| GET | `/api/instruments/preflight/NAME` | Validate an installed archive before activation. |
| POST | `/api/instruments/activate/NAME` | Activate and wait for readiness. |
| POST | `/api/instruments/control/start`, `/api/instruments/control/stop`, `/api/instruments/control/restart` | Control the loaded service. |
| POST | `/api/instruments/default/NAME` | Select the boot default. |
| GET | `/api/system/status` | Read instruments, current/last operation and board health. |
| GET | `/api/system/diagnostics` | Download status and diagnostic logs. |
| WebSocket | `/api/events` | Follow status and operation changes. |
| WebSocket | `/api/logs/koheron/events?cursor=CURSOR` | Follow instrument logs. |
| GET | `/api/logs/koheron/tail?cursor=CURSOR` | Fetch a resumable log batch over HTTP. |

### Host command behavior

- `make deploy` returns after readiness; `make run` follows logs after startup.
  Their default `TIMEOUT=120` covers board operations, not the preceding build.
  A timeout does not cancel an operation already started on the board. Inspect
  `make status HOST=... FORMAT=json` before retrying.
- `make status` and `make logs` default to `TIMEOUT=10`. `make logs` returns a
  finite snapshot; `LOG_LIMIT` defaults to 100 and accepts 1–2000.
- Log entries carry microsecond timestamps, priority and a truncation flag.
  HTTP and WebSocket endpoints share cursors for reconnect/fallback. Expired
  cursors return `reset: true` with recent history.

See the [management API](os/api/README.md),
[upload preservation (#697)](https://github.com/Koheron/koheron-sdk/pull/697),
[activation rollback (#699)](https://github.com/Koheron/koheron-sdk/pull/699) and
[management API/UI changes (#836)](https://github.com/Koheron/koheron-sdk/pull/836).

## Board support and shared hardware drivers

- Added KR260 board support, including its ZynqMP/AArch64 boot and runtime
  configuration, and ported ZedBoard support to the V1 build/runtime layout.
- Retained ALPHA15, ALPHA250, ALPHA250-4, ALPHA250-1G, ALPHA250-4-1G and Red
  Pitaya definitions. A board definition's presence is not evidence that every
  build or hardware combination has been tested.
- Removed the MicroZed, MYD-C7Z015, TE0745 and ZC706 definitions. Custom projects
  based on them need their own maintained board configuration.
- Updated board definitions for the V1 `board.mk`, kernel, U-Boot and
  device-tree build inputs. Custom board ports need to supply the new boot
  and runtime configuration as well as FPGA part/board settings.
- Corrected ALPHA250-4 DDR configuration to match the ALPHA250 settings.
- Hardened shared SPI/I2C operations: validate sizes/settings, report short
  transfers and avoid replaying incomplete bus transactions automatically.
- Split EEPROM writes at 32-byte page boundaries, preventing a write from
  wrapping within a page.
- Corrected shared monitoring drivers, including XADC scaling/temperature
  initialization and INA230 signed/scaled measurements.
- Fixed precision ADC worker lifetime and shutdown. Precision DAC validation
  rejects invalid/non-finite values while preserving calibration behavior.

See [board definitions](boards),
[removed boards](https://github.com/Koheron/koheron-sdk/commit/9cde8778),
[shared driver fixes (#820)](https://github.com/Koheron/koheron-sdk/pull/820) and
[ALPHA250-4 DDR settings (#869)](https://github.com/Koheron/koheron-sdk/pull/869).

## Tests and validation

- Added a common `make check` entry point for configuration validation and
  local regressions covering Python, packaging and OS/FPGA/server/web build
  rules. Fixtures isolate generated outputs from normal build caches.
- Added focused suites for runtime/transport behavior, hardware-bus edge cases,
  memory and CMA ownership, image creation, board management and shared browser
  components. Some have separate runners or environment requirements; they
  are not all part of `make check`.
- GitHub Actions currently validates example configurations. It does not run
  Vivado, all host regressions or hardware tests. The move from CircleCI itself
  also reached V0.
- Separated local checks from `make test`, which runs a selected project's
  script against a board. Host test success, a successful build, routed timing
  and hardware operation are separate results.

See [host checks](python/koheron/check.py), [server tests](server/tests),
[board management tests](os/api/tests), [web tests](web/tests) and
[the CI workflow](.github/workflows/ci.yml).

## Changes already in V0

If upgrading from the fixed [V0.24 tag](https://github.com/Koheron/koheron-sdk/tree/d02dcc63280974bb8166c7fd070715baa7c6d155),
you also get the following SDK changes. They are already present in the V0
`master` baseline used above:

- Python virtual-environment setup and Docker mounts for projects outside the
  SDK tree.
- C++ HTTP management helpers and operation validation
  ([#710](https://github.com/Koheron/koheron-sdk/pull/710)).
- BRAM initialization from `.mem` files
  ([#647](https://github.com/Koheron/koheron-sdk/pull/647)).
- Shared ALPHA precision DAC reset and Red Pitaya MIO19 fixes
  ([#669](https://github.com/Koheron/koheron-sdk/pull/669),
  [#716](https://github.com/Koheron/koheron-sdk/pull/716)).
- The fix for a TypeScript socket leak on empty vector replies
  ([#658](https://github.com/Koheron/koheron-sdk/pull/658)).
- The switch from CircleCI to GitHub Actions
  ([#674](https://github.com/Koheron/koheron-sdk/pull/674)).

For a custom fork, compare file contents between the pinned revisions as well
as reading the commit history. The branches contain merges and backports, so
a commit list alone can overstate their functional differences:

```sh
# SDK file changes between the compared snapshots
git diff d37dd08061d0fc7e63d04b327c1fc530e054186c ab2c618648407386f9da21961cd98bf3a5a6724c -- Makefile make.py build.sh version .github make server python os docker fpga boards web

# V1 history beyond the V0 snapshot, including intermediate changes
git log --oneline d37dd08061d0fc7e63d04b327c1fc530e054186c..ab2c618648407386f9da21961cd98bf3a5a6724c

# Changes already made on V0 since its last tagged release
git log --oneline V0.24..d37dd08061d0fc7e63d04b327c1fc530e054186c
```

For the steps to port a project, continue with [MIGRATING.md](MIGRATING.md).
