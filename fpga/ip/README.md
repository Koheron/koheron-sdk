# Configurable FPGA IP

This directory houses reusable subsystems packaged for the Vivado IP Catalog,
with AXI control and customization parameters that select their hardware
features. The first subsystem is the [DDS phase modulator](./awg_v1_0/).

## Directory conventions

Use one versioned directory per IP, following the existing `<name>_v<major>_<minor>`
packaging convention. Keep RTL sources and `core_config.tcl` at its root to work
with the SDK's current packager. Put simulation scripts and testbenches in
`tests/`, and document the parameters, register map and interfaces in its README.

Keep reusable RTL primitives in `fpga/cores/`, Tcl assemblies in `fpga/modules/`,
and board-specific wiring and demonstration instruments in `boards/` and
`examples/`. A subsystem here should expose digital interfaces and configurable
widths; DAC pin formatting and analog voltage conversion belong to its board
adapter and software driver.

## SDK integration

The existing `CORES` variable accepts explicit source directories, including
paths under `fpga/ip/`. Once an IP's RTL and `core_config.tcl` are implemented,
an instrument can select it in `config.mk`, for example:

```make
CORES += $(SDK_PATH)/fpga/ip/awg_v1_0
```

`make cores` packages selected IP into the instrument's temporary IP repository.
Directory basenames must be unique across selected IP and cores because the
packager uses those basenames for its output directories. RTL module names
should also avoid collisions with other SDK cores.

## Reuse in examples

Each subsystem should provide an integration Tcl helper alongside its RTL.
The helper instantiates the packaged IP, applies customization parameters,
connects AXI clocks and resets, and assigns its register region using the
example's `memory.yml`. The example supplies its sample clock, output
connections and optional trigger or modulation inputs.

An example should only need to select the IP in `config.mk`, declare a register
region in `memory.yml`, call the helper in `block_design.tcl`, and select the
shared server driver. Keep the register definitions and signal-generation logic
shared rather than copying them into individual examples. Helpers should take
explicit instance and memory-region names so multiple instances are possible.

Register integration Tcl helpers as build dependencies through `TCL_FILES`;
files under `fpga/ip/` are not covered by the existing `fpga/lib/*.tcl` wildcard.
Provide a standalone demonstration instrument before integrating a new
subsystem into existing measurement examples.

## Customization and resource use

Build-time parameters select channels, supported features and arithmetic
precision. Use conditional RTL generation so disabled features are absent from
the synthesized hardware. AXI registers control the features included in the
build; disabling a feature at runtime does not remove its hardware.

Expose parameter validation and dependencies through Vivado customization.
Document supported combinations and report utilization and timing for
representative configurations before claiming resource or throughput results.
