# AWG and ALPHA250 control timing

Qualified source: `7243c0c2`; baseline: V1 `9885ee45`.
Vivado/Vitis 2026.1, Ubuntu 24.04, global synthesis. Clock constraints,
timing exceptions and implementation directives are unchanged.

The AWG uses bit checks for the full-turn depth/duty limit. The ALPHA250
analyzer enables the existing `PREDECODE_WRITES` AXI decoder. Register values,
AXI response timing and sample latency are preserved. AWG validation margin
improves from 33 to 259 ps in the AWG-only build; analyzer write-enable margin
improves from 109 to 542 ps after predecode, saving 57 control-bank LUTs.
The final analyzer critical path is FIR control routing at 143 MHz.

All five instruments pass `all validate` with `ENFORCE_TIMING=1`, including
setup, hold, pulse width and bus skew. Whole-instrument margins in ns:

| Instrument | Baseline setup | Final setup | Final hold |
| --- | ---: | ---: | ---: |
| ALPHA250 FFT | 0.046443 | 0.064919 | 0.002452 |
| ALPHA250 phase modulator | 0.130014 | 0.079283 | 0.014208 |
| ALPHA250 phase-noise analyzer | 0.033029 | 0.137618 | 0.042408 |
| Red Pitaya FFT | 0.345557 | 0.469704 | 0.008399 |
| Red Pitaya phase-noise analyzer | 0.003451 | 0.428812 | 0.013792 |

Placement regressions remain: phase-modulator setup decreases from 130 to
79 ps; FFT hold decreases from 29 to 2.5 ps. The latter is very tight.
These measurements qualify the reported builds, not other placements or tools.

Passed regressions: AWG limits against the original comparison at all 17
phase widths (32–48), real vendor DDS integration, packaged AWG profiles,
and AXI equivalence with an independent scoreboard at 12 and 17 words
(10,025 cycles, 3,798 writes, 2,743 reads each). Analyzer history, prefilter,
rounding, response and block-design checks pass. All five ZIPs pass CRC,
artifact/manifest checks, ARM ELF identification and board-copy equality.

Rebuild with each instrument's configuration, for example:

```sh
make -j6 CFG=examples/alpha250/phase-noise-analyzer/config.mk \
    VIVADO_VERSION=2026.1 N_CPUS=6 ENFORCE_TIMING=1 all validate
```

Hardware tests: no board deployment or testing.
