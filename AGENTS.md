# Koheron SDK

- V1 is under development. Boards ship with V0; use a V1 OS image for V1 instruments.
- Reference host: Ubuntu 24.04 with Vivado/Vitis 2025.1. `make setup` does not install Xilinx tools.
- Read the README and an example for the user's board. Use `make help`, `make doctor` and `make list` to get oriented.
- Use `make info CFG=... FORMAT=json` for resolved inputs and artifact paths; `make doctor TARGET=...` checks task-specific requirements.
- `make check [CFG=...]` runs local host checks. `make test CFG=... HOST=...` runs an instrument hardware script.
- Select instruments with `CFG=.../config.mk`; `memory.yml` defines the hardware/software memory map.
- `make run` streams logs over WebSocket, with HTTP fallback. Ctrl+C stops the stream, leaving the instrument running.
- `make deploy CFG=... HOST=...` returns after readiness; `TIMEOUT` bounds board operations. Read `make status HOST=... FORMAT=json` after a timeout before retrying. `make logs HOST=...` returns a snapshot.
- Report build checks and hardware tests separately.
