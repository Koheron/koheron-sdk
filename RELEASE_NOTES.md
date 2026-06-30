# Koheron SDK V1 release notes

V1 is a breaking update of the Koheron SDK. Main changes introduced or substantially improved in V1:

- Replaced the old `CONFIG=.../config.yml` flow with `CFG=.../config.mk`.
- Split instrument configuration into `config.mk` for build settings and `memory.yml` for memory maps, registers, Linux device mappings and parameters.
- Added FPGA Manager based runtime loading using `.bit.bin` bitstreams and `pl.dtbo` device-tree overlays.
- Added generation of PL overlays from Vivado/Vitis device-tree output, `memory.dtsi` and optional `override.dtsi` files.
- Added `memory.dtsi` generation from `memory.yml` for selected Linux-visible memory regions.
- Added support for memory mappings through devices such as `/dev/mem`, `/dev/mem_wc` and `/dev/uio`.
- Updated the development environment to Ubuntu 24.04 and Vivado/Vitis 2025.1.
- Updated generated OS images to Ubuntu 24.04.3 with Xilinx 2025.1 components.
- Added FIT image generation with kernel, base device tree and board overlay.
- Reworked board support around `board.mk`, board overlays, board-local drivers, board-local cores, FSBL hooks and U-Boot patches.
- Modernized the C++ server build with C++20, GCC 13, ccache, LTO and precompiled headers.
- Improved generated-server dependency tracking to reduce unnecessary rebuilds.
- Added generated `drivers.json` as an explicit instrument artifact.
- Migrated several board and instrument drivers from header-only implementations to `.hpp` / `.cpp` pairs.
- Migrated example instruments from `config.yml` to the new `config.mk` / `memory.yml` layout.
- Added or updated helper targets such as `doctor`, `validate`, `list` and `examples`.
- Improved parallel build output with synchronized Make logs.
- Added release metadata in generated images, including build ID, board, instrument, Git revision and component versions.
- Reworked root filesystem customization through a staged overlay tree.
- Improved image packaging and SD-card flashing flow.

Notes:

- V1 OS images do not support legacy V0 instruments.
- Some subsystems already existed on `master` and were improved rather than introduced from scratch, including the HTTP API, web staging, Docker build flow, generated server interfaces and ZynqMP support.
- See `MIGRATING.md` for porting existing instruments to V1.