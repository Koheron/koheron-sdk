# AWG configuration-validation timing

Baseline: V1 merge commit `9885ee452a5326b9fcc84be76a5d79ed7420c621`
(PR #862). Measurements use Vivado/Vitis 2026.1 on Ubuntu 24.04,
global synthesis and the existing implementation directives.

The depth and duty limit is exactly `2**PHASE_WIDTH`. Validation now tests
higher bits and the full-turn bit together with nonzero lower bits, replacing
the numerical comparison's carry chain. Zero and exactly one full turn remain
valid. AXI responses, byte/register masks, mailbox coherence, commit latency,
sample pipelines and clocks are unchanged.

## Routed builds

All five AWG instruments passed `all validate` with `ENFORCE_TIMING=1`,
including setup, hold, pulse-width and bus-skew checks. These are
whole-instrument minima in nanoseconds:

| Instrument | Baseline setup | Revised setup | Revised hold |
| --- | ---: | ---: | ---: |
| ALPHA250 FFT | 0.046443 | 0.064919 | 0.002452 |
| ALPHA250 phase modulator | 0.130014 | 0.079283 | 0.014208 |
| ALPHA250 phase-noise analyzer | 0.033029 | 0.109299 | 0.030740 |
| Red Pitaya FFT | 0.345557 | 0.469704 | 0.008399 |
| Red Pitaya phase-noise analyzer | 0.003451 | 0.428812 | 0.013792 |

Reproduce with each instrument's configuration, for example:

```sh
make -j6 CFG=examples/alpha250/phase-noise-analyzer/config.mk \
    VIVADO_VERSION=2026.1 N_CPUS=6 ENFORCE_TIMING=1 all validate
```

## Routed ALPHA250 paths and limits

In the phase-noise analyzer, the worst depth/duty-validation path to a
mailbox enable improves from 0.033 ns to 0.259 ns, with five LUT levels and
no carry blocks instead of 12 levels containing eight CARRY4s. The AWG's
minimum setup margin rises from 0.033 ns to 0.145 ns. Whole-instrument
setup improves from 0.033029 ns to 0.109299 ns. The critical path moves to
the separate AXI control-register write decoder:
`system_i/ctl/axi_ctl_register/inst/awaddr_q_reg[4]` to
`regs_flat_reg[352]/CE`. It is outside the AWG validation logic.

ALPHA250 FFT validation has 0.789 ns margin, and phase-modulator validation
has 1.098 ns. Their whole-instrument critical paths remain elsewhere in the
AWG: FFT's increment-to-wrap path and phase-modulator's exact-phase bypass.
Placement changes reduce phase-modulator setup margin from 0.130014 ns to
0.079283 ns and FFT hold margin from 0.029124 ns to 0.002452 ns. Both pass
strict checks, but these are material margin regressions. Qualification is
limited to these routed builds and does not guarantee other placements or
Vivado versions.

The routed two-channel ALPHA250 FFT AWG uses 4,772 LUTs and 7,032 flip-flops,
versus 4,693 LUTs and 7,052 flip-flops in the baseline. Both use 10 RAMB36s,
six DSPs and 547 SRLs. No pipeline register or DSP stage was added.

## Behavioral and package checks

- `run_control.tcl`: all 17 phase widths (32–48), zero/full-turn boundaries,
  each lower bit above the limit for depth and duty, random settings,
  masked high bits, rejected commits retaining the mailbox, accepted settings
  reaching the sample domain, and coherent commits with a stopped sample clock.
  The same expanded test passes against the original numerical comparison.
- `run_sim.tcl`: real vendor DDS, all shapes, native precision, AXI/CDC,
  settings tags, reset and mute timing.
- `run_subsystem.tcl`: packaged one-channel, two-channel and no-sine simulation
  and synthesis, including source pruning and no unresolved subcores.
- Five final instrument packages: CRC, archive members matched to current
  artifacts, manifests, ARM ELF and identical board copies.

## Hardware

No board was deployed or tested for this change.
