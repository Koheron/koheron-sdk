# AWG timing qualification

Measured with Vivado/Vitis 2026.1 on Ubuntu 24.04, global synthesis and the
existing implementation directives. Original baseline: V1 commit
`ae3dccbbc842d2b7117e24684e4fdc6a246727b4`. The intermediate baseline
`6f8dcc4d` includes wrap lookahead and Gaussian retiming. No clock constraints,
exceptions, sample rates or pipeline latencies were changed.

## Routed instrument builds

All five instruments using the AWG passed `all validate` with
`ENFORCE_TIMING=1`, including setup, hold, pulse-width and bus-skew checks.
These are whole-instrument minima, rounded to picoseconds:

| Instrument | Original setup (ns) | Wrap/Gaussian setup (ns) | Final setup (ns) | Final hold (ns) |
| --- | ---: | ---: | ---: | ---: |
| ALPHA250 FFT | -0.028 | 0.073 | 0.046 | 0.029 |
| ALPHA250 phase modulator | 0.036 | 0.062 | 0.130 | 0.039 |
| ALPHA250 phase-noise analyzer | 0.058 | 0.009 | 0.033 | 0.034 |
| Red Pitaya FFT | 0.240 | 0.426 | 0.346 | 0.018 |
| Red Pitaya phase-noise analyzer | 0.333 | 0.096 | 0.003 | 0.013 |

In ALPHA250 phase-noise analyzer, waveform selection now ends at preserved
operand-capture registers with 1.319 ns margin. The minimum DDS-to-scaler
margin improves from 0.009 ns to 0.125 ns; the remaining path is the exact-phase
bypass. Both DSP multiplier pipeline stages remain enabled. The instrument's
critical path moves to the 200 MHz AXI settings-to-mailbox enable path:
`shadow_reg[8][0]` to `mailbox_reg[273]/CE`, with 0.033 ns margin.

In ALPHA250 FFT, the AWG's minimum setup margin improves from 0.073 ns to
0.125 ns. Its wrap path has 0.196 ns margin, compared to -0.028 ns originally.
The complete instrument is now limited by a path inside the FFT core, at
0.046 ns. The ALPHA250 phase modulator has 0.130 ns overall margin.

Overall margin is not uniformly higher: placement and routing also change
unmodified paths. The full ALPHA250 phase-noise analyzer's 0.033 ns margin
remains tight. Red Pitaya phase-noise analyzer's overall margin decreases
from 0.096 ns to 0.003 ns on an unmodified CIC path (26 CARRY4s); this also
remains very tight and is a timing-margin regression outside the scaler.
Passing these builds does not establish margin for other
placements, tool releases or configurations.

The routed two-channel AWG in ALPHA250 FFT uses 4,693 total LUTs and 7,052
flip-flops, versus 4,693 LUTs and 7,006 flip-flops before the scaler retiming.
Original V1 uses 4,651 LUTs and 6,866 flip-flops. All three use 10 RAMB36s,
six DSPs and 547 SRLs. Thus the scaler adds 46 flip-flops and no LUTs, DSPs
or BRAMs in this implementation. The Gaussian retiming adds 84 pipeline bits
per enabled channel before synthesis optimizations.

Reproduce each build using its configuration:

```sh
make -j6 CFG=examples/alpha250/fft/config.mk \
    VIVADO_VERSION=2026.1 N_CPUS=6 ENFORCE_TIMING=1 all validate
```

The other configurations are `examples/alpha250/phase-modulator/config.mk`,
`examples/alpha250/phase-noise-analyzer/config.mk`,
`examples/red-pitaya/fft/config.mk` and
`examples/red-pitaya/phase-noise-analyzer/config.mk`.

## Simulation and package checks

- `run_phase_accumulator.tcl`: carry boundaries, 20,000 changing input pairs,
  and every phase width from 32 to 48; accumulator, offset, BPSK and Gaussian
  sample values/timing checked against the original equations, including
  changing seeds, resets and restarts.
- `run_scale.tcl`: full-width mathematical reference at phase widths 32–48
  and modulation widths 16/24, signed rounding, depth extremes, all shapes,
  20,000 changing settings, validity gaps, resets and restarts. Seven-cycle
  data/valid/enable timing is preserved. The same arithmetic oracle passes
  against the original scaler. Default-profile synthesis verifies three DSPs,
  enabled MREG/PREG stages and preserved operand capture.
- `run_sim.tcl`: all modulation shapes, native precision, real vendor DDS,
  AXI, CDC, reset and mute timing.
- `run_control.tcl`: coherent commits with a stopped sample clock at phase
  widths 32, 33 and 48.
- `run_subsystem.tcl`: packaged one-channel, two-channel and no-sine
  simulation and synthesis; no unresolved subcores and correct pruning.
- All five instrument ZIPs: CRC, required artifacts, every archive member
  matched to its build output, manifest, ARM ELF architecture and board copy.

## Hardware

No board was deployed or tested for this change. Routed static timing and
simulation do not replace hardware verification.
