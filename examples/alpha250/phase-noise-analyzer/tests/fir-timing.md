# ALPHA250 analyzer FIR control timing

Baseline: write-predecode revision
`7243c0c2199f05c621c8300ed116bf06335e2ad7` (PR #864).
Measured with Vivado/Vitis 2026.1 on Ubuntu 24.04, global synthesis and the
existing Explore implementation directives. Clock constraints, timing
exceptions, FIR configuration, coefficients and RTL are unchanged.

## Change

The 143 MHz compensation FIR's registered FIFO not-full flag drives 2,975
loads, including 71 DSP clock enables. Its baseline critical path ends at a
DSP48E1 CEB2 pin, with 6.055 ns data delay: 0.433 ns logic and 5.622 ns
routing (93%). Its setup margin is 0.137618 ns.

After placement, `tcl/replicate_fir_control.tcl` selects registered FIR nets
with more than 512 loads that reach DSP clock enables, sets a
`FORCE_MAX_FANOUT` target of 64 and calls
`phys_opt_design -force_replication_on_nets`. Selection uses connectivity
rather than private vendor register names. The current design selects one
net and Vivado adds eight register copies. The target guides physical
grouping; it does not guarantee a maximum fanout of 64.

This copies the existing register without adding a pipeline stage or changing
filter arithmetic, throughput, sample timing, reset or backpressure behavior.
The analyzer-local hook runs before routing, where forced replication is
supported. The existing shared post-route DAC skew and hold repairs remain
in use. `FPGA_IMPL_TCL` tracks the helper so changes rerun implementation
while retaining synthesis.

## Routed results

| Measurement | Before replication | With replication |
| --- | ---: | ---: |
| Whole-instrument setup (ns) | 0.137618 | 0.201360 |
| Whole-instrument hold (ns) | 0.042408 | 0.042408 |
| FIR minimum setup (ns) | 0.138 | 0.281 |
| FIR LUTs | 1,529 | 1,529 |
| FIR flip-flops | 3,888 | 3,896 |
| FIR DSP48E1s | 71 | 71 |
| FIR SRLs | 1,227 | 1,227 |

FIR minimum margin approximately doubles. Its new worst path is from a
replica to a DSP48E1 CED pin, still dominated by routing: 6.087 ns data
delay, including 5.708 ns routing. This is a different endpoint from the
baseline path.

The whole-instrument critical path moves to the 250 MHz AWG phase scaler:
the sine DDS output to `exact_phase_reg[4][44]_srl5/D`, with 3.644 ns data
delay and 13 logic levels (11 CARRY4s and two LUTs). AWG configuration
validation retains 0.328 ns margin; AXI settings-register enables retain
0.552 ns margin. Further FIR replication alone will not remove this AWG
carry path. Results qualify this routed build, not other placements or tools.

## Build checks

`all validate` passes with `ENFORCE_TIMING=1`: setup, hold, pulse width and
all 13 bus-skew constraints. The existing warning about 14 inputs and 41
outputs without delay constraints remains; these checks do not qualify
unconstrained external I/O timing.

The existing analyzer regressions pass: AXI equivalence and independent
scoreboard (10,025 cycles, 3,798 writes, 2,743 reads), phase history,
prefilter arithmetic, rounding and ideal response. Block-design checks
verify the replication hook, predecoded settings bank, clocks/resets,
independent DAC stimulus and DMA connectivity. All five AWG instrument
packages pass CRC, artifact/manifest checks, ARM ELF identification and
equality with their board copies. Only this analyzer was rebuilt for this
local hook; the other four instruments retain their
[earlier qualification](../../../../fpga/ip/awg_v1_0/tests/config-timing-margin.md).

Reproduce:

```sh
PNA_VIVADO_SETTINGS=/tools/Xilinx/2026.1/Vivado/settings64.sh \
    bash examples/alpha250/phase-noise-analyzer/tests/run-fpga.sh
make -j6 CFG=examples/alpha250/phase-noise-analyzer/config.mk \
    VIVADO_VERSION=2026.1 N_CPUS=6 ENFORCE_TIMING=1 all validate
vivado -mode batch -source examples/alpha250/phase-noise-analyzer/tests/check_fpga.tcl \
    -tclargs tmp/examples/alpha250/phase-noise-analyzer/fpga/phase-noise-analyzer.xpr
```

## Hardware

No board was deployed or tested for this change.
