# Packaged FPGA subsystems

This directory houses reusable subsystems packaged for the Vivado IP Catalog,
with AXI control or AXI4-Stream interfaces and customization parameters.
The packages include their control RTL and vendor-IP configurations, so examples
can instantiate a complete function instead of wiring its internals.

| IP | Interface | Function |
| --- | --- | --- |
| [DDS phase modulator](awg_v1_0/README.md) | AXI control, digital sample outputs | One or two complete DDS phase-modulator channels |
| [axis_accumulator](axis_accumulator_v1_0/README.md) | AXI4-Stream slave/master | Double-buffered float32 frame accumulation |

## Directory conventions

Use one versioned directory per IP, following the existing `<name>_v<major>_<minor>`
packaging convention. Keep RTL sources and `core_config.tcl` at its root to work
with the SDK's current packager. Put simulation scripts and testbenches in
`tests/`, and document the parameters, register map and interfaces in its README.

Keep reusable RTL primitives in `fpga/cores/`, Tcl assemblies in `fpga/modules/`,
and board-specific wiring and demonstration instruments in `boards/` and
`examples/`. A subsystem here should expose digital interfaces and configurable
widths; DAC pin formatting and analog voltage conversion belong to its board
wiring and software driver.

## SDK integration

The existing `CORES` variable accepts explicit source directories, including
paths under `fpga/ip/`. An instrument selects its IP in `config.mk`, for example:

```make
CORES += $(SDK_PATH)/fpga/ip/awg_v1_0
CORES += $(SDK_PATH)/fpga/ip/axis_accumulator_v1_0
```

`make cores` packages selected IP into the instrument's temporary IP repository.
The packager runs an optional `package_ip.tcl` before importing RTL and embedded
XCI. Vendor output products are regenerated for the target device. An optional
`custom_gui.tcl` supplies the Vivado customization UI.

Directory basenames must be unique across selected IP and cores because the
packager uses those basenames for its output directories. RTL module names
should also avoid collisions with other SDK cores.

## Reuse in examples

Examples can instantiate a packaged IP directly with the normal `cell` command,
just as they instantiate Xilinx catalog IPs. The accumulator FFT examples use
this approach; their block designs provide the clock, reset, stream and recorder
connections.

For subsystems with AXI registers, an optional integration Tcl helper can apply
customization parameters, connect AXI clocks and resets, and assign the register
region declared in the example's `memory.yml`. The DDS phase modulator provides
such a helper. Keep register definitions and signal-generation logic shared.
Helpers should take explicit instance and memory-region names so multiple
instances are possible.

Register integration Tcl helpers as build dependencies through `TCL_FILES`;
files under `fpga/ip/` are not covered by the existing `fpga/lib/*.tcl` wildcard.
Document standalone catalog use and verify representative integrations before
migrating measurement examples. Board clocks, physical I/O and instrument-specific
readout stay in the example.

## Standalone distribution

Run `fpga/vivado/export_ip.tcl` with the core directory, packaging part and output
directory. It exports a portable ZIP containing the catalog metadata, GUI,
RTL and embedded XCI. Consumers extract it, add that directory under
**Settings → IP → Repository**, then add and customize the IP normally.

## Customization and resource use

Build-time parameters select channels, supported features and arithmetic
precision. Use conditional RTL generation so disabled features are absent from
the synthesized hardware. Where present, AXI registers control the features
included in the build; disabling a feature at runtime does not remove its hardware.

Expose parameter validation and dependencies through Vivado customization.
Document supported combinations and report utilization and timing for
representative configurations before claiming resource or throughput results.
