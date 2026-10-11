# AWG timing qualification

Measured with Vivado/Vitis 2026.1 on Ubuntu 24.04, global synthesis and the
existing implementation directives. Baseline: V1 commit
`ae3dccbbc842d2b7117e24684e4fdc6a246727b4`. No clock constraints, exceptions,
sample rates or pipeline latencies were changed.

## Routed instrument builds

All five instruments using the AWG passed `all validate` with
`ENFORCE_TIMING=1`, including setup, hold, pulse-width and bus-skew checks.
These are whole-instrument minima, rounded to picoseconds:

| Instrument | Setup before (ns) | Setup after (ns) | Hold after (ns) |
| --- | ---: | ---: | ---: |
| ALPHA250 FFT | -0.028 | 0.073 | 0.007 |
| ALPHA250 phase modulator | 0.036 | 0.062 | 0.042 |
| ALPHA250 phase-noise analyzer | 0.058 | 0.009 | 0.030 |
| Red Pitaya FFT | 0.240 | 0.426 | 0.011 |
| Red Pitaya phase-noise analyzer | 0.333 | 0.096 | 0.016 |

The ALPHA250 FFT phase-register-to-wrap-register path improves from -0.028 ns
to 0.269 ns. Its critical path moves to waveform generation. The wrap path
has 0.199 ns margin in the ALPHA250 phase modulator and 0.300 ns in the
ALPHA250 phase-noise analyzer.

Overall margin is not uniformly higher: placement and routing also change
unmodified paths. The ALPHA250 phase-noise analyzer remains limited by the
modulation sine LUT to scaler path; the Red Pitaya phase-noise analyzer is
limited by its FIR. In particular, the ALPHA250 analyzer's 0.009 ns setup
margin remains tight. Passing these builds does not establish margin for
other placements, tool releases or configurations.

The routed two-channel AWG in ALPHA250 FFT changes from 4,651 to 4,693 total
LUTs and from 6,866 to 7,006 flip-flops. It retains 10 RAMB36s, six DSPs and
547 SRLs. The Gaussian retiming adds 84 pipeline bits per enabled channel
before synthesis optimizations; its five-clock latency and exact samples are
preserved.

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
