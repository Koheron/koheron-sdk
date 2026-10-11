# SDK development

[SDK quick start](../README.md#quick-start) · [FPGA builds](../fpga/README.md) ·
[OS images](../os/README.md)

Run commands from the SDK root. Replace the example `CFG` with an instrument
for your board and `HOST` with the board's IP address or hostname.

## Configuration

`CFG` selects the instrument's `config.mk`:

- `config.mk` defines its name, board, FPGA cores, constraints, drivers and web assets.
- `memory.yml` defines the memory map, registers, Linux mappings and parameters;
  it generates Tcl, C++ and device-tree definitions.

`SDK_PATH` is the SDK root; `PROJECT_PATH` contains `config.mk`. `make info`
reports actual Make settings, overrides, inputs, toolchains and artifact paths
without building or creating directories.

```sh
make info CFG=examples/red-pitaya/fft/config.mk FORMAT=json
make doctor TARGET=web CFG=examples/red-pitaya/fft/config.mk
```

Vivado/Vitis default to `/tools/Xilinx/2025.1`. Select another installed release
with `VIVADO_VERSION=...`, or override `VIVADO_PATH` and `VITIS_PATH`.
Compiler and runtime OS selections are independent; see
[compiler settings](../docker/README.md) and [OS settings](../os/README.md#settings).
`VERBOSE=1` adds build details.

## Make targets

Build and deployment commands require `CFG=.../config.mk`. `make help` lists
targets; `make list` lists examples.

| Command | Purpose |
| --- | --- |
| `make` or `make all` | Build the FPGA bitstream, server and web assets, then package the instrument ZIP. |
| `make fpga` | Build the selected Vivado bitstream. |
| `make timing` | Check routed FPGA timing. |
| `make block_design` | Open the Vivado block design interactively. |
| `make open_project` | Open the generated Vivado project. |
| `make server` | Compile the C++ TCP/WebSocket server. |
| `make web` | Build the instrument's TypeScript/CSS assets. |
| `make os` | Build the board's Linux root filesystem. |
| `make uboot` | Build the bootloader; select its release with `UBOOT_VERSION=...`. |
| `make image` | Build a bootable SD card image and release ZIP. |
| `make flash` | Write an existing release ZIP to an SD card; run `image` first. |
| `make run` | Build, upload and start the instrument, then stream logs. |
| `make deploy` | Build, upload, preflight and activate, then return after confirmed readiness. |
| `make status HOST=...` | Read the V1 board's service, instrument and operation state. |
| `make logs HOST=...` | Return recent server logs across invocations; `LOG_LIMIT=100` by default. |
| `make info CFG=...` | Print resolved configuration, inputs, toolchains and artifact paths. |
| `make doctor TARGET=... [CFG=...]` | Check local requirements for the requested task. |
| `make check [CFG=...]` | Validate selected or all example configurations and run host regressions. |
| `make test CFG=... HOST=...` | Run the instrument's hardware test script. |
| `make validate CFG=...` | Validate `config.mk` and `memory.yml`. |
| `make copy CFG=... DEST=...` | Copy an instrument's sources and set its name from the destination directory. |
| `make list` or `make examples` | List example instruments. |

`status` and `logs` need no CFG or build tools. For flashing and board-selected
builds in `build.sh`, see [OS image builds](../os/README.md#building-and-flashing).

## Deployment and logs

```sh
make deploy CFG=examples/red-pitaya/fft/config.mk HOST=192.168.1.100
make status HOST=192.168.1.100 FORMAT=json
make logs HOST=192.168.1.100
```

Deployment uses the V1 management API: validate package identity and board
compatibility, upload, preflight activation and confirm the running name,
version and service state. Failures retain rollback details.

`TIMEOUT=120` is the default upload, activation and readiness budget;
the preceding build has its own duration. A timeout does not cancel an operation
already started on the board. Read `make status HOST=...` before retrying.
Status and logs default to `TIMEOUT=10`; override it on the command line.

`make logs` returns a finite snapshot, with UTC timestamps in text output.
`LOG_LIMIT` accepts 1–2000 entries. `make run` streams logs over WebSocket,
reconnects from the last cursor and falls back to HTTP polling when necessary.
Ctrl+C closes the stream and leaves the instrument running.
The [Python upload/run API](https://www.koheron.com/software-development-kit/documentation/v1/python-api/#upload-and-run-an-instrument)
also returns after deployment for scripts.

## JSON for scripts and agents

`info`, `doctor`, `check`, `deploy`, `status` and `logs` accept `FORMAT=json`.
Successful stdout contains one JSON object with `schema_version: 1`.
Diagnostics and board commands include `ok` and return nonzero on failure;
deployment build output goes to stderr. JSON logs retain the original
microsecond timestamps and other entry fields.

Use `info` to discover resolved inputs and artifact paths, then `doctor` and
`check` for local validation. Use `deploy` for a bounded board operation and
`status` to inspect readiness or a timed-out deployment. See
[AGENTS.md](../AGENTS.md) for working conventions.

## Diagnostics and tests

`doctor` checks local requirements for `all`, `web`, `server`, `fpga`, `image`,
`os`, `uboot`, `api` or `check`. The default is `all`. Web/server checks skip
Xilinx tools. With CFG, it uses the same resolved settings as `info`, including
overrides such as `MEMORY_YML`.

`make check` validates all examples and runs Python, packaging, server dependency
and web build regressions. With CFG, validation uses that configuration;
regressions use isolated fixtures. Reports include configuration and test counts,
with per-suite counts and durations in JSON. Checks use SDK sources and existing
host dependencies without installing packages, synthesizing FPGA designs or
contacting a board. If dependencies are missing:

```sh
make python_requirements
make koheron_python
```

`HOST_PYTHON=...` selects another prepared interpreter for inspection,
diagnostics, host checks and management commands.

`make test CFG=... HOST=...` runs `test.py` or `python/test.py` from the selected
instrument. `INSTRUMENT_TEST=...` overrides the script. Hardware scripts may
change acquisition/output settings or open plots. Report host/build checks
separately from hardware results.

Pull requests and pushes check host workflows/configurations, cached build
dependencies, C++ logging, the compiled C++23 management runtime and management UI.
Full server, sanitizer, instrument build, DSP and browser tests remain available
locally when needed. See [the CI workflow](../.github/workflows/ci.yml) for exact suites.

## Creating a new instrument

```sh
make copy CFG=examples/alpha250/fft/config.mk DEST=examples/alpha250/my-instrument
make validate CFG=examples/alpha250/my-instrument/config.mk
make -j CFG=examples/alpha250/my-instrument/config.mk
```

`copy` sets `NAME := my-instrument` for the archive and stored instrument.
It refuses an existing destination or the source name and skips generated
metadata and dependency caches. Board settings, `VERSION`, driver names and
shared SDK references are preserved. The API class remains `FFT`; update
hard-coded client instrument names such as `connect(host, 'fft')`.

## Instrument packages

The instrument ZIP is `tmp/<project>/<NAME>.zip`, also copied to
`tmp/<board>/instruments/<NAME>.zip`. It contains:

- The FPGA Manager bitstream (`.bit.bin`) and device-tree overlay (`pl.dtbo`).
- The original Vivado bitstream (`.bit`) for reference.
- The compiled server (`serverd`) and driver metadata (`drivers.json`).
- Built web assets.
- A `version` file and `instrument.json` identifying board, architecture and
  runtime requirements for deployment preflight.

ZIP names use letters, digits, dots, underscores and hyphens, without a leading
or trailing dot or underscore. Deployment checks local identity and metadata
size before contacting the board; preflight verifies compatibility and payloads.

## Rebuilds and cached outputs

Driver order determines RPC IDs. Changing the ordered driver list regenerates
interfaces and metadata together. Python generator edits and complete server
compiler flags are tracked as build inputs. Web builds use current compiler
sources and refresh the Docker builder when its inputs change.
Web source membership is tracked even when a file disappears without a config
edit. Selecting an older HTML or CSS source with the same filename refreshes
the copied asset without recompiling unchanged TypeScript. Instrument archives
and board copies are published atomically. Missing board copies and switches
between projects with the same name use the selected archive. Version and
compatibility metadata overrides refresh the package.

See [FPGA builds](../fpga/README.md) for synthesis caching, timing qualification
and Tcl dependencies, and [OS image builds](../os/README.md#building-and-flashing)
for image caching.

## Repository layout

```text
boards/    # Board definitions, boot components and helper makefiles
docker/    # Reproducible Docker builders
examples/  # Reference instruments with config.mk and memory.yml
fpga/      # FPGA cores, IP, Tcl helpers and build logic
os/        # Linux images, board management runtime and web pages
python/    # Build tools, runners and client library
server/    # C++ server, drivers and build rules
web/       # Shared instrument UI components
```
