# Migrating from Koheron SDK 0.x to V1

V1 changes the build configuration and FPGA-loading runtime. The
[ALPHA250 VCO example](#worked-example-alpha250-vco) below shows how to port an
existing instrument with minimal source changes. Feature improvements to other
reference instruments are described separately in the
[release notes](RELEASE_NOTES.md#instruments-and-interfaces).

## Runtime compatibility

Boards ship with V0. The [quick start](./README.md#quick-start) builds a V1 image;
the [OS guide](./os/README.md#building-and-flashing) covers installation.

- V1 instruments may still run on some V0 OS images through the legacy
  `/dev/xdevcfg` path.
- V1 OS images use Linux FPGA Manager and device-tree overlays to load the
  programmable logic.
- V1 OS images do not support legacy V0 instruments.

## Move from `/dev/xdevcfg` to FPGA Manager

V1 images use the Linux FPGA Manager overlay interface in place of
`/dev/xdevcfg`. Instruments package `pl.dtbo` and `<instrument>.bit.bin` to load
the FPGA and configure PL clocks.

Device-tree overrides support Linux drivers such as `/dev/uio*` and `/dev/mem_wc*`.

## `config.yml` is split between `config.mk` and `memory.yml`

```bash
# V0
make CONFIG=examples/alpha250/vco/config.yml

# V1
make CFG=examples/alpha250/vco/config.mk
```

- `config.mk`: name, board, cores, drivers and web assets.
- `memory.yml`: memory map, registers, Linux mappings and parameters; generates
  `memory.tcl`, `memory.hpp` and `memory.dtsi`.

## Worked example: ALPHA250 VCO

Compare the [V0 VCO at `d37dd080`](https://github.com/Koheron/koheron-sdk/tree/d37dd08061d0fc7e63d04b327c1fc530e054186c/examples/alpha250/vco)
with the [V1 example](examples/alpha250/vco/README.md). Both versions use the
same `block_design.tcl`, Python `test.py` and RPC method bodies. The port
preserves memory addresses, region sizes, register order and parameters.

The input checks and Python cleanup are shared by both versions and are
independent of V1. The migration changes the build configuration and C++
memory namespace; the V1 example also uses the preferred constructor shown
below. Shared board and SDK code has changed, so check FPGA timing and output
behavior when porting a custom design.

### Convert the build configuration

Move the build settings from `config.yml` into
[`config.mk`](examples/alpha250/vco/config.mk):

| V0 setting | V1 location or setting |
| --- | --- |
| `name`, `version` | `NAME`, `VERSION` in `config.mk` |
| `board` | `BOARD_PATH` in `config.mk` |
| `cores` | `CORES`; include the board's `cores.mk` for shared cores |
| `drivers` | `DRIVERS`; include the board's `drivers.mk` for shared drivers |
| `xdc` | `XDC` in `config.mk` |
| `web` | `WEB_FILES` in `config.mk` |
| `memory`, `parameters` | `memory.yml` |
| `control_registers`, `status_registers`, `ps_control_registers`, `ps_status_registers` | `registers` inside the corresponding `memory.yml` region |

The complete VCO build configuration is:

```make
NAME := vco
VERSION := 0.2.1
ENFORCE_TIMING := 1

BOARD_PATH := $(SDK_PATH)/boards/alpha250

MEMORY_YML = $(PROJECT_PATH)/memory.yml

XDC += $(SDK_PATH)/boards/alpha250/config/ports.xdc

include $(SDK_PATH)/boards/alpha250/cores/cores.mk
CORES += $(SDK_PATH)/fpga/cores/axis_variable_v1_0
CORES += $(SDK_PATH)/fpga/cores/right_shifter_v1_0

include $(SDK_PATH)/boards/alpha250/drivers/drivers.mk
DRIVERS += $(PROJECT_PATH)/vco.hpp

WEB_FILES += $(SDK_PATH)/web/index.html
```

Use `$(SDK_PATH)` for SDK files and `$(PROJECT_PATH)` for instrument files,
including paths previously written as `./vco.hpp`. The board's driver include
lists both headers and any required `.cpp` implementations. If you maintain
custom board drivers, carry those changes into the corresponding files and
source list. The SDK already supplies `web/main.css` and `web/koheron.ts`, so
VCO adds only `web/index.html` to `WEB_FILES`.

### Preserve the memory map

Move all memory regions and parameters into
[`memory.yml`](examples/alpha250/vco/memory.yml). Nest each register list under
its region: `control_registers` under `control`, `status_registers` under
`status`, and the two `ps_*_registers` lists under their matching `ps_*` regions.
For example, these excerpts describe the same control region:

```yaml
# V0: config.yml
memory:
  - name: control
    offset: '0x60000000'
    range: 4K

control_registers:
  - mmcm
  - precision_dac_ctl
  - precision_dac_data[2]
  - phase_incr[4]
  - vco_gain[2]
```

```yaml
# V1: memory.yml
memory:
  - name: control
    offset: 0x6000_0000
    range: 4K
    registers:
      - mmcm
      - precision_dac_ctl
      - precision_dac_data[2]
      - phase_incr[4]
      - vco_gain[2]
```

Preserve register names, array lengths and order so generated register offsets
stay the same. VCO keeps `fclk0: 200000000`, `adc_clk: 250000000`, `n_adc: 2` and
`adc_width: 16`. Its Tcl and C++ code can continue using the same generated
memory, register and parameter definitions.

### Update the driver

The preferred V1 style in [`vco.hpp`](examples/alpha250/vco/vco.hpp) replaces
the V0 context constructor with direct access to the memory service:

```diff
-#include <context.hpp>
+#include "server/hardware/memory_manager.hpp"

-    VCO(Context& ctx) : ctl(ctx.mm.get<mem::control>()) {}
+    VCO() : ctl(hw::get_memory<mem::control>()) {}

-    Memory<mem::control>& ctl;
+    hw::Memory<mem::control>& ctl;
```

The two RPC method bodies are shared by the polished V0 and V1 examples. V1
still supports `#include <context.hpp>`, constructors taking `Context&` and
`ctx.mm.get<...>()`, although those constructors are deprecated. For an
incremental port, qualifying the memory types is sufficient; changing the
constructor is recommended modernization.

The Python script still uses `connect(host, 'vco')` and the `@command()`
interface. This example needs no instrument-specific client changes; other
instruments need their own RPC and behavior comparison.

### Build and check the port

Use the [V1 host setup](README.md#requirements), including the current C++23
server toolchain, and run from the SDK root:

```sh
make info CFG=examples/alpha250/vco/config.mk FORMAT=json
make doctor CFG=examples/alpha250/vco/config.mk
make check CFG=examples/alpha250/vco/config.mk
make -j CFG=examples/alpha250/vco/config.mk
make timing CFG=examples/alpha250/vco/config.mk
```

`check` validates the configuration and runs host regressions; it does not
synthesize the FPGA or test a board. The build produces
`tmp/examples/alpha250/vco/vco.zip`. The polished V1 example sets
`ENFORCE_TIMING := 1`, so builds also fail on routed timing violations.
`timing` can recheck the routed result separately.

After installing a V1 OS image on the ALPHA250, deploy to its address:

```sh
make deploy CFG=examples/alpha250/vco/config.mk HOST=192.168.1.100
make test CFG=examples/alpha250/vco/config.mk HOST=192.168.1.100
```

The VCO script selects the onboard reference clock and sets channel 0 to a
10 MHz offset with gain 4. It demonstrates control calls without asserting
measured output behavior. Follow the [VCO README](examples/alpha250/vco/README.md)
for connections and control ranges, and verify the output separately. Report
hardware results separately from host, build and timing checks. See the
[development guide](docs/development.md) for deployment status, timeouts and logs.

## Device-tree overlay

`pl.dtbo` is compiled from `pl_wrap.dts`:

```dtsi
/dts-v1/;
/plugin/;
/include/ "pl.dtsi"         /* Generated by Vivado/Vitis */
/include/ "memory.dtsi"     /* Generated from memory.yml */
/include/ "override.dtsi"   /* User defined */
```

### Example 1: Map a reserved region of RAM using the mem-wc driver

```yaml
memory:
  - name: ram
    offset: 0x1E00_0000
    range: 32M
    compatible: "koheron,mem-wc-1.0"
    dev: /dev/mem_wc
```

`compatible: "koheron,mem-wc-1.0"` generates this `memory.dtsi` fragment:

```dtsi
/* memory.dtsi */
&amba {
    #address-cells = <1>;
    #size-cells    = <1>;
    kmem_ram: ram@1E000000 {
        compatible = "koheron,mem-wc-1.0";
        reg = <0x1E000000 0x02000000>;
    };
};
```

`dev: /dev/mem_wc` selects `/dev/mem_wc@0x1E000000` for server memory access;
the default is `/dev/mem`.

### Example 2: Interrupt access in userspace

```yaml
# memory.yml
memory:
  - name: fifo
    range: 64K
    offset: 0x43C0_0000
    dev: /dev/uio
```

```dtsi
/* override.dtsi */
&axi_fifo_mm_s_0 {
    compatible = "generic-uio";
    status = "okay";
    linux,uio-name = "irq-fifo";
    linux,driver-override = "uio_pdrv_genirq";
};
```

The override binds the FIFO to `/dev/uio*`. `dev: /dev/uio` selects the device
matching the region's offset and range.
