# Packaged FPGA subsystems

These catalog IPs include their control RTL and vendor-IP configurations, so
examples can instantiate a complete function instead of wiring its internals.
Add the subsystem directory to `CORES` in the instrument's `config.mk`; the SDK
packager runs `package_ip.tcl` before importing the sources and embedded XCI.
Vendor output products are regenerated for the target device.

To distribute an IP for use in an independent Vivado project, run
`fpga/vivado/export_ip.tcl` with the core directory, packaging part and output
directory. It exports a portable ZIP containing the catalog metadata, GUI,
RTL and embedded XCI. Consumers extract it, add that directory under
**Settings → IP → Repository**, then add and customize the IP normally.

| IP | Interface | Function |
| --- | --- | --- |
| [axis_accumulator](axis_accumulator_v1_0/README.md) | AXI4-Stream slave/master | Double-buffered float32 frame accumulation |

Board clocks, physical I/O and instrument-specific readout stay in the example.
