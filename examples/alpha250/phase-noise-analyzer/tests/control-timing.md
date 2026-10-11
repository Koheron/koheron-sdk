# ALPHA250 analyzer control-write timing

This follows the AWG validation improvement in revision
`3f54c4c78c3f2cd81aaf0b00fdbcc6fb2b147294` (PR #864).
Measured with Vivado/Vitis 2026.1 on Ubuntu 24.04, global synthesis and the
existing implementation directives. No clock constraints or timing exceptions
were changed.

## Change

Enable the existing shared `axi_ctl_register` option `PREDECODE_WRITES=1`
for this analyzer's 12-word, 250 MHz settings bank. ALPHA250 DPLL and
ALPHA250-4 phase-noise analyzer already use this implementation. The word
select is captured on AW acceptance ahead of the existing write commit,
replacing the legacy dynamic write-index path. Data, byte strobes, AW/W
ordering, response timing, reads and reset behavior are preserved. There is
no change to sample rate, measurement latency or register addresses.

## Routed results

The previous critical path runs from `awaddr_q_reg[4]` to
`regs_flat_reg[352]/CE`: 0.109 ns slack, four logic levels including two
CARRY4s. The new word-select-to-enable path has 0.542 ns margin; the
minimum over every settings-register enable has 0.542 ns margin. All 12
word-select registers are present, and the unused AW address registers are
removed. The write-enable path has one LUT5 level, versus four levels with
two CARRY4s before. The new global critical path is inside the vendor
`system_i/fir/U0` core on the 143 MHz processing clock: 6.055 ns data delay,
including 5.622 ns routing (about 93%), and 0.137618 ns slack. It is outside
the control-write decoder. The whole-instrument margin remains limited by
this FIR route; these measurements qualify this implementation only.

| Measurement | AWG-only baseline | With write predecode |
| --- | ---: | ---: |
| Whole-instrument setup (ns) | 0.109299 | 0.137618 |
| Whole-instrument hold (ns) | 0.030740 | 0.042408 |
| AXI control bank LUTs | 289 | 232 |
| AXI control bank flip-flops | 485 | 483 |

`all validate` passed with `ENFORCE_TIMING=1`, including setup, hold,
pulse-width and all bus-skew checks. Other instruments' RTL/configurations
are unchanged by this step; their earlier qualification is in the
[AWG timing report](../../../../fpga/ip/awg_v1_0/tests/config-timing-margin.md).

## Regressions

The DPLL's existing AXI equivalence test is parameterized without changing its
17-word default and reused at the analyzer's 12-word size. Each run passes
10,025 cycles, 3,798 writes and 2,743 reads, comparing every control word,
handshake, response and response cycle against the legacy decoder and an
independent scoreboard. Coverage includes independent AW/W arrivals, byte
strobes, misaligned/out-of-range addresses, response backpressure, simultaneous
reads/writes and resets during transactions.

The analyzer's phase-history, prefilter, rounding and ideal-response regressions
also pass. Block-design checks assert the 12-word predecoded bank and verify
clock/reset, extraction, independent DAC stimulus and DMA connectivity.
The instrument ZIP passes CRC, artifact/manifest checks, ARM ELF identification
and equality with its board copy.

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
