# Koheron SDK

- V1 is under development. Boards ship with V0; use a V1 OS image for V1 instruments.
- Reference host: Ubuntu 24.04 with Vivado/Vitis 2025.1. `make setup` does not install Xilinx tools.
- Read the README and an example for the user's board. Use `make help`, `make doctor` and `make list` to get oriented.
- Select instruments with `CFG=.../config.mk`; `memory.yml` defines the hardware/software memory map.
- `make run` keeps streaming logs. Ctrl+C stops the stream, leaving the instrument running.
- Report build checks and hardware tests separately.
