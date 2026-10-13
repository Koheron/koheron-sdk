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

`SDK_PATH` is the SDK root; `PROJECT_PATH` contains `config.mk`.
`make info CFG=... FORMAT=json` reports resolved settings, inputs and artifact
paths without building.
Equivalent paths to an instrument inside the SDK share the same build cache.
An instrument outside the SDK uses `tmp/.external/<path-hash>` so its artifacts
stay inside the selected `TMP` directory.

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
| `make clean CFG=...` | Remove only the selected instrument's cache. |
| `make clean_all` | Remove the selected `TMP` cache directory; no CFG needed. |

`status` and `logs` need no CFG or build tools. For image installation and board
presets, see [OS image builds](../os/README.md#building-and-flashing).

## Deployment and logs

```sh
make deploy CFG=examples/red-pitaya/fft/stemlab-125-14.mk HOST=192.168.1.100
make status HOST=192.168.1.100 FORMAT=json
make logs HOST=192.168.1.100
```

`deploy` builds the package, checks board compatibility, activates it and
returns after readiness. `run` builds, uploads and starts the instrument, then
streams logs; Ctrl+C leaves it running. The [board management guide](../os/api/README.md)
documents the API contracts.

`TIMEOUT=120` is the default upload, activation and readiness budget;
the preceding build has its own duration. A timeout does not cancel an operation
already started on the board. Read `make status HOST=...` before retrying.
Status and logs default to `TIMEOUT=10`; override it on the command line.

`make logs` returns a finite snapshot with UTC timestamps. `LOG_LIMIT` accepts
1–2000 entries. `make run` reconnects its log stream from the last cursor and
falls back to HTTP polling when needed.
The [Python upload/run API](https://www.koheron.com/software-development-kit/documentation/v1/python-api/#upload-and-run-an-instrument)
also returns after deployment for scripts.

## Machine-readable output

`info`, `doctor`, `check`, `deploy`, `status` and `logs` accept `FORMAT=json`.
Successful stdout contains one JSON object with `schema_version: 1`.
Diagnostics and board commands include `ok` and return nonzero on failure;
deployment build output goes to stderr. JSON logs retain the original
microsecond timestamps and other entry fields.

## Diagnostics and tests

`doctor` checks local requirements for `all`, `web`, `server`, `fpga`, `image`,
`os`, `uboot`, `api` or `check`. The default is `all`. Web/server checks skip
Xilinx tools. With CFG, it uses the same resolved settings as `info`, including
overrides such as `MEMORY_YML`.

`make check` validates all examples and runs Python and packaging checks plus OS,
FPGA, server and web build regressions. With CFG, it validates that configuration;
regressions use isolated fixtures. It uses existing host dependencies, without
synthesis or board access. Install missing Python dependencies with
`make python_requirements`
and `make koheron_python`.

`HOST_PYTHON=...` selects another prepared interpreter for inspection,
diagnostics, host checks and management commands.

`make test CFG=... HOST=...` runs `test.py` or `python/test.py` from the selected
instrument. `INSTRUMENT_TEST=...` overrides the script. Hardware scripts may
change acquisition/output settings or open plots. Report host/build checks
separately from hardware results.

[CI](../.github/workflows/ci.yml) validates example configurations. Run
`make check` locally for the broader host regressions.

## Creating a new instrument

```sh
make copy CFG=examples/alpha250/fft/config.mk DEST=examples/alpha250/my-instrument
make validate CFG=examples/alpha250/my-instrument/config.mk
make -j CFG=examples/alpha250/my-instrument/config.mk
```

`copy` sets `NAME := my-instrument` and preserves the board, version, drivers
and shared references. It does not rewrite the API class or client names such
as `connect(host, 'fft')`.

## Instrument packages

For an SDK instrument, the ZIP is `tmp/<project>/<NAME>.zip`, also copied to
`tmp/<board>/instruments/<NAME>.zip`. It contains:

- The FPGA Manager bitstream (`.bit.bin`) and device-tree overlay (`pl.dtbo`).
- The original Vivado bitstream (`.bit`) for reference.
- The compiled server (`serverd`) and driver metadata (`drivers.json`).
- Built web assets.
- A `version` file and `instrument.json` identifying board, architecture, SDK
  commit and runtime requirements for deployment preflight.

ZIP names use letters, digits, dots, underscores and hyphens, without a leading
or trailing dot or underscore. Deployment checks local identity and metadata
size before contacting the board; preflight verifies compatibility and payloads.

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
