# Koheron SDK V1 release notes

V1 is a major, breaking update of the Koheron SDK. It keeps the general idea of the existing SDK - Make-based FPGA, Linux, C++ server and web builds - but changes the configuration model, runtime FPGA loading mechanism, operating-system image generation and many internal build dependencies.

These notes focus on features that were added or substantially improved in V1 compared with the `master` branch. Some subsystems already existed on `master` and are listed here only when V1 changes their behavior, packaging, or maintainability in a meaningful way.

## Highlights

- New `CFG=.../config.mk` project selection model.
- New `config.mk` + `memory.yml` split replacing the monolithic `config.yml` flow.
- Runtime FPGA loading moved to Linux FPGA Manager with instrument-specific device-tree overlays.
- Instrument archives now include FPGA-manager artifacts: `.bit.bin` and `pl.dtbo`.
- Device-tree overlay generation added for PL, board overlays and selected memory regions.
- Build system updated for Ubuntu 24.04 and Vivado/Vitis 2025.1.
- OS image generation updated for Ubuntu 24.04.3 and Xilinx 2025.1 components.
- FIT-based kernel packaging added for generated images.
- Server build modernized with C++20, GCC 13, ccache, LTO, stricter generated dependencies and precompiled headers.
- Example instruments migrated to the new configuration layout.
- Board support reorganized around board-local makefiles, overlays, drivers, FSBL hooks and U-Boot patches.

## Breaking changes

### V1 is not backward-compatible with V0.x

V1 instruments and V1 OS images are not drop-in replacements for the previous `master` / V0.x flow.

The main incompatibilities are:

- V1 uses `CFG=.../config.mk` instead of `CONFIG=.../config.yml`.
- V1 OS images use Linux FPGA Manager and device-tree overlays instead of the legacy `/dev/xdevcfg` loading path.
- Legacy V0 instruments are not supported by V1 OS images.
- Existing instruments must be migrated to the `config.mk` + `memory.yml` structure.

See `MIGRATING.md` for migration guidance.

## Configuration and project model

### New `CFG=config.mk` build entry point

The top-level build now selects an instrument with:

```bash
make CFG=examples/alpha250/fft/config.mk
```

instead of the previous:

```bash
make CONFIG=examples/alpha250/fft/config.yml
```

The top-level `Makefile` validates that `CFG` is provided for build targets and reports a clear error if the selected file does not exist.

### Split configuration files

The old `config.yml` model has been split into two files:

- `config.mk` contains build settings: instrument name, version, board path, Vivado cores, C++ drivers, XDC files and web assets.
- `memory.yml` contains memory maps, register lists, Linux device mappings and build-time parameters.

This makes the build graph more explicit and lets Make track build-time dependencies directly.

### Improved generated-file handling

Several generated files are now written only when their content changes. This reduces unnecessary rebuilds caused by timestamp-only changes, especially for generated memory, server and metadata files.

## FPGA build flow

### FPGA Manager artifacts

V1 generates a binary bitstream suitable for Linux FPGA Manager:

- `<instrument>.bit.bin`

This is generated from the Vivado `.bit` file using `bootgen -process_bitstream bin`.

### Device-tree overlay packaging

Each instrument archive now includes:

- `pl.dtbo`, the PL device-tree overlay.
- `<instrument>.bit.bin`, the FPGA-manager bitstream.
- The original `.bit` file for reference/debugging.

The archive also includes the server executable, generated driver JSON, web assets and version metadata.

### Generated memory Tcl

`memory.yml` now drives generation of `memory.tcl`, which is used by the FPGA build flow. This replaces the old approach where the FPGA configuration was extracted from the monolithic YAML file.

### Improved Vivado build logging

The FPGA makefile now routes Vivado output through a log filter script for clearer build output.

### Core build dependencies improved

Core generation rules now depend on a broader set of HDL file extensions and the core configuration Tcl file. This improves correctness for Verilog, SystemVerilog, VHDL and header changes.

## Device tree and runtime loading

### Linux FPGA Manager instead of `/dev/xdevcfg`

The V1 runtime path uses Linux FPGA Manager. V1 OS images no longer rely on the legacy `/dev/xdevcfg` device.

This is one of the main architectural changes in V1.

### PL overlay generation

V1 builds a PL overlay from:

- the generated Vivado/Vitis PL device-tree output;
- generated `memory.dtsi`;
- optional user `override.dtsi`;
- the wrapper `pl_wrap.dts`.

This allows the instrument archive to carry both the bitstream and the device-tree description needed to load and bind it at runtime.

### `memory.dtsi` generation

`memory.yml` can now emit device-tree fragments for selected memory regions. For example, a memory region can declare a `compatible` string and be exposed through a Linux driver such as `koheron,mem-wc-1.0`.

### `/dev/mem_wc` and `/dev/uio` mappings

The memory map can specify Linux device mappings such as:

- `/dev/mem`
- `/dev/mem_wc`
- `/dev/uio`

This connects the generated C++ memory map to the runtime Linux device used for the mapping.

## Operating-system and image generation

### Toolchain update

V1 targets a newer build environment:

- Ubuntu 24.04 development host.
- Vivado/Vitis 2025.1.
- GCC 13 cross compilers.

The previous `master` flow targeted older Vivado and Ubuntu combinations.

### Ubuntu 24.04.3 root filesystem

Generated images are based on Ubuntu 24.04.3. The rootfs build downloads the Ubuntu base tarball and verifies it against the release `SHA256SUMS` file.

### FIT image generation

V1 builds a FIT image, `kernel.itb`, containing:

- the Linux kernel;
- the base device tree;
- the board overlay.

This replaces the older direct `uImage`-style boot packaging used by the previous Zynq flow.

### Board overlay support

Board-specific overlays are now supported through `board.dtso` / `board.dtbo`. This replaces many older board device-tree patch files and makes board-specific changes more explicit.

### Rootfs overlay staging

The root filesystem customization is now assembled through a staged overlay tree and bundled as an overlay tarball. The overlay includes:

- Koheron API files;
- web assets;
- systemd units;
- nginx configuration;
- uWSGI configuration;
- Koheron server initialization files;
- default instrument archive;
- release metadata;
- optional grow-rootfs service;
- MOTD customization.

### Release metadata

V1 generates release metadata such as:

- release name;
- build ID;
- board;
- instrument;
- Zynq type;
- kernel, U-Boot and device-tree versions;
- Koheron SDK version;
- Git commit, branch, tag and dirty status;
- UTC generation time.

This metadata is installed in the image under Koheron release/manifest files.

### Improved SD-card flashing flow

The image build produces a release zip and provides a `flash` target using the Python flashing helper. The flashing flow supports device/vendor/model filtering to reduce the risk of writing to the wrong device.

## Server and driver build

### C++20 and GCC 13

The server build now uses C++20 and GCC 13 cross compilers.

### ccache and LTO

The server compiler command uses ccache and link-time optimization to improve rebuild speed and runtime performance.

### Precompiled headers

The server build now generates and uses a precompiled header. This reduces compile times for the heavily templated/generated server code.

### Refactored source layout

The server build now separates sources into clearer categories, including:

- network;
- executor;
- runtime;
- hardware;
- utilities;
- main.

Hardware support objects such as SPI, I2C, FPGA-manager and Zynq clock handling are built as part of the server runtime.

### Improved generated-driver dependencies

V1 separates generated files that depend only on the driver list from generated files that depend on driver contents. This helps avoid unnecessary rebuilds when only timestamps change.

### Generated `drivers.json`

The instrument archive now includes a generated `drivers.json` file. It is produced by compiling and running a small `drivers_json_dump` helper, rather than relying only on generated headers.

### Driver `.cpp` support emphasized

Board and instrument drivers can now be split more cleanly between headers and implementation files. Several board drivers were migrated from large header-only implementations to `.hpp` + `.cpp` pairs.

## Web and HTTP runtime

The HTTP API and web staging flow already existed on `master`, but V1 improves how they are packaged into generated images.

### Runtime web assets

The rootfs overlay stages web assets under `/usr/local/www`, including dashboard pages, shared assets, generated TypeScript output and static Koheron assets.

### Runtime API packaging

The Koheron API is staged under `/usr/local/api` and served through uWSGI behind nginx.

### Instrument upload/run flow

The `run` target now uses the Python `koheron.instrument_runner` helper instead of direct `curl` commands. This keeps the user-facing target but centralizes the upload/run logic in the Python package.

## Board support

### Board makefiles

Board definitions are now organized around `board.mk` files that define the board name, FPGA part, Zynq type, U-Boot configuration, FSBL patch path and board overlay.

### Board-local cores and drivers

Several boards now provide local `cores/cores.mk` and `drivers/drivers.mk` files. This makes board-specific FPGA cores and C++ drivers explicit and reusable across instruments.

### Alpha board updates

The Alpha250, Alpha250-1G, Alpha250-4, Alpha250-4-1G and Alpha15 support files were updated for the V1 build model.

Notable improvements include:

- board overlays;
- U-Boot patches and defconfigs moved into board-specific patch trees;
- FSBL hook files moved under board-specific patch directories;
- driver implementations split into `.cpp` files where appropriate;
- build image configuration files added.

### Red Pitaya updates

Red Pitaya support was updated for the V1 board model, including board overlay support, board-local cores, drivers and U-Boot patches.

### KR260 / ZynqMP work

V1 adds a KR260 board directory and updates the ZynqMP build path around the V1 OS/image model.

### Legacy board cleanup

Several older board directories and legacy patch files were removed from the active V1 tree. This simplifies the maintained board set but means users of removed boards must either stay on V0.x or port those boards to the V1 structure.

## Example instruments

Many examples were migrated from `config.yml` to the V1 layout:

- `config.mk`
- `memory.yml`
- updated block-design Tcl dependencies;
- updated driver declarations;
- updated web file declarations;
- generated overlay support.

Updated examples include Alpha250, Alpha250-4 and Alpha15 instruments such as FFT, ADC/DAC BRAM, ADC/DAC DMA, decimator, signal analyzer and VCO designs.

## Developer workflow

### New helper targets

V1 adds or improves helper targets such as:

- `doctor`, to check host tools and optionally validate a selected configuration;
- `validate`, to validate the selected `CFG` and `memory.yml`;
- `list` / `examples`, to list available examples;
- improved Python dependency setup through a virtual environment.

### Parallel-build improvements

The top-level Makefile enables synchronized output for parallel builds, reducing interleaved logs when using `make -j`.

### Distro-aware setup scripts

Setup scripts were reorganized with Debian and Red Hat helper paths for host dependency installation.

## Notes for users upgrading from `master`

- Existing instruments must be ported from `config.yml` to `config.mk` + `memory.yml`.
- Runtime loading assumptions must be updated from `/dev/xdevcfg` to FPGA Manager.
- Device-tree changes should be expressed as overlays rather than patches where possible.
- Board support may need porting if the board was removed or still relies on legacy patch files.
- The OS image flow now expects the newer Ubuntu/Vivado/Vitis environment.
- Some features that existed on `master`, such as the HTTP API, web staging, Docker build flow, generated server interfaces and ZynqMP support, are improved in V1 but are not brand-new features.

## Related documentation

- `README.md` gives the V1 quick-start and build overview.
- `MIGRATING.md` explains how to port existing V0.x instruments to V1.
