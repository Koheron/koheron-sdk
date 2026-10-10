# Wide phase-history feedback

The shared `phase_unwrapper` offers registered carry lookahead for histories
wider than 32 bits when `LOOKAHEAD_HISTORY=1`, `PIPELINED_OVERFLOW=1` and
`PIPELINED_HISTORY=0`. The option defaults to zero for existing users.
The shared PNA builder enables it above 125 MHz, including ALPHA250-4.
The ALPHA250 PNA and DPLL retain their existing split-history pipeline;
Red Pitaya retains the smaller accumulator at 125 MHz.

The low word adds the signed phase increment. The remaining high word can
only increment, decrement or hold. For each four-bit high-word group, two
registers record whether all preceding high-word bits are zero or one.
These predicates let the late low-word carry/borrow reach short local XOR
logic instead of propagating through the entire history accumulator.

The lookahead accumulator owns both the history register and its predicates;
its caller supplies only a step, reset and enable. The predicates describe
the **current** history and update on the same edge. Their next values are predicted from the
old predicates and comparisons with one and minus two, without waiting for
the full sum. The first group's empty-prefix predicates are constants.
The 64-bit/17-bit configuration adds 22 state registers per accumulator.
Output phase and frequency samples keep their exact values and clock
latency; the existing delayed sticky overflow flag is unchanged.

## Clock and configuration contract

There is no added pipeline stage. For an input sampled on edge `n`, assuming
history accumulation is enabled, the corresponding outputs update as follows:

| Difference mode | Frequency | Full history, with or without lookahead | Split history |
| --- | --- | --- | --- |
| `FUSED_DIFFERENCE=0` | `n+1` | `n+2` | `n+3` |
| `FUSED_DIFFERENCE=1` | `n` | `n+1` | `n+2` |

All modes accept a new sample every clock. `rst` synchronously clears history
and sticky overflow and takes precedence over `acc_on`. Neither signal stops
or resets the input-difference pipeline: inputs continue to advance while
history is disabled or reset. Full history holds while `acc_on=0`; split
history still exposes its previously accepted sample on the following edge.
Outputs initialize to zero, including before the first clock.

`PIPELINED_HISTORY=1` takes precedence over `LOOKAHEAD_HISTORY` and requires
`DOUT_WIDTH>32`, `DIN_WIDTH<32` and `PIPELINED_OVERFLOW=1`. Otherwise lookahead
is selected only when `PIPELINED_OVERFLOW=1`, `DOUT_WIDTH>32` and
`DOUT_WIDTH>DIN_WIDTH+1`; other combinations retain the conventional adder.
`PIPELINED_OVERFLOW` delays only the sticky overflow indication by one clock
in full-history mode. `CANONICAL_INPUT` is used only with `FUSED_DIFFERENCE`
and requires sign-extended angles in `[-pi,pi)`.

The core requires `DIN_WIDTH>=3` and `DOUT_WIDTH>=DIN_WIDTH`; fused difference
requires `DIN_WIDTH>=4`. Invalid widths and unsupported split-history
configurations produce explicit diagnostics.

## RTL checks

```sh
VIVADO_SETTINGS=/tools/Xilinx/2026.1/Vivado/settings64.sh \
  fpga/tests/phase_unwrapper/run.sh
```

The runner checks:

- Exhaustive small accumulators and randomized 32/64/65-bit cases against
  independent signed addition, with every cached predicate checked after
  every clock. Directed cases cover all carry/borrow boundaries, minimum
  and maximum signed steps, resets and pauses (153,425 checked cycles).
- Full 64-bit phase histories with 16-bit and 24-bit input phases against
  the original arithmetic and with lookahead disabled, including exact
  sample latency and both signed overflow boundaries.
- An independent sample model checks initialization, exact phase/frequency
  latency, continuous throughput and reset/enable behavior across twelve
  configurations, including fused/canonical input, split-history precedence
  and the conventional-adder fallbacks.
- The existing 32-bit overflow-pipeline, ALPHA250 PNA split-history and
  ALPHA250 DPLL arithmetic regressions, plus phase-headroom/range-guard checks.

Tests that seed a long history directly also seed its cached predicates;
normal operation initializes both through the synchronous reset.

## Implementation checks

Use the normal build target, with the same clocks and Vivado settings for
the baseline and changed RTL:

```sh
make -j4 CFG=examples/alpha250-4/phase-noise-analyzer/config.mk \
  TMP=tmp/rtl-unwrapper-check VIVADO_VERSION=2026.1 MODE=development \
  N_CPUS=4 ENFORCE_TIMING=1 fpga
```

Measurements on 2026-10-10 compare the original core at `dfb4ce6f` with
the initial registered-lookahead core at `00c58453`. No clock, constraint or implementation
strategy changes are part of this RTL change.

| ALPHA250-4 PNA, 250 MHz | Original | Lookahead |
| --- | ---: | ---: |
| Overall worst setup slack | -0.069659 ns | +0.048918 ns |
| Worst setup slack within phase unwrappers | -0.070 ns | +0.111 ns |
| Overall worst hold slack, lookahead build | — | +0.028740 ns |
| Synthesis LUT primitives | 24,019 | 24,079 |
| Synthesis CARRY4 cells, before Unisim transformation | 1,698 | 1,590 |
| Additional flip-flops | — | 88 |
| Routing command elapsed time, including final hold route | 127 s | 65 s |

The changed full build produced a bitstream with strict timing enforcement.
An independent build with the final selectable configuration reproduced all
144 implementation-stage checksums and the exact setup/hold margins.
The worst setup path moved into the vendor CORDIC. Its approximately 49 ps
margin is still small. The routing times are observations from these runs,
not a guaranteed speedup on every machine or instrument.

Enabling the prefix cache on Red Pitaya was also tested. It met setup/hold
timing (+0.228 ns / +0.007 ns), but its changed placement exposed a -0.138 ns
bus-skew failure between AWG mailbox registers. Routing took more passes.
It is therefore not enabled there: the extra history registers did not offer
a useful tradeoff in this 125 MHz PNA. No AWG timing constraint was weakened
or waived.

The initial configuration builds used the same `make fpga` flags above:

| Instrument | History architecture | Setup / hold slack | Strict build |
| --- | --- | --- | --- |
| ALPHA250-4 PNA | Registered lookahead | +0.048918 / +0.028740 ns | Pass, 18 bus-skew constraints checked |
| Red Pitaya PNA | Original full history | +0.332713 / +0.020674 ns | Pass, 15 bus-skew constraints checked |
| ALPHA250 PNA | Existing split history | +0.066792 / +0.009590 ns | Pass, 13 bus-skew constraints checked |
| ALPHA250 DPLL | Existing split history | -0.077903 / +0.028 ns | Fail, reproduced with original source |

Red Pitaya's original-source baseline passed at +0.209866 / +0.015564 ns.
The final selection preserves the original accumulator architecture; the
different placement margin is not evidence of an arithmetic-path speedup.
The ALPHA250 PNA has identical synthesis checksums and timing margins to
its original-source baseline.

The DPLL baseline and final configuration both fail at exactly -0.077903 ns
setup slack (TNS -0.112270 ns). The limiting path is from
`cordic0/phase_extractor/inst/rotation[7].pipeline.xr_reg_rep_bsel[17]` to
`residual_completion.completion/interpolation/dsp/B[6]`, outside the phase
history. The initial lookahead selection leaves its synthesized circuit unchanged. This existing
phase-extractor closure problem is not fixed or waived here.

### Accumulator state ownership

The follow-up refactor places the history register inside the lookahead
accumulator alongside its cached predicates. It adds no register stage or
arithmetic primitive. All seven RTL suites pass, including the independent
twelve-configuration latency test. A separate comparison against the RTL at
`00c58453` also checks phase, frequency and overflow on every clock for those
twelve configurations, with identical results.

Full builds on the PR's current `V1` base use the normal `make fpga` command
above, default global synthesis, and unchanged clocks, constraints and Vivado
strategies. ALPHA250-4 passes at +0.046698 ns setup / +0.030740 ns hold, including
all 18 bus-skew checks. Its worst unwrapper path is +0.098 ns; the limiting
overall path remains in the vendor CORDIC. Flip-flop and arithmetic-primitive
counts match the initial lookahead implementation. Red Pitaya passes at
+0.332713 / +0.020674 ns with all 15 bus-skew checks, reproducing the initial
implementation's margins. Both board integration checks pass. ALPHA250 PNA
also reproduces its original +0.066792 / +0.009590 ns margins and passes all
13 bus-skew checks. Its board integration check passes as well.
The DPLL build reproduces the original extractor-to-DSP setup failure exactly:
WNS -0.077903 ns, TNS -0.112270 ns. Strict timing enforcement blocks its
bitstream. All four synthesis primitive tables match the initial selection;
this refactor introduces no extra registers or arithmetic cells.

The board integration checks also verify the selected accumulator architecture:

```sh
vivado -mode batch -nolog -nojournal -notrace \
  -source examples/alpha250-4/phase-noise-analyzer/tests/check_fpga.tcl \
  -tclargs tmp/rtl-unwrapper-check/examples/alpha250-4/phase-noise-analyzer/fpga/phase-noise-analyzer.xpr
```

Use the Red Pitaya `tests/check_fpga.tcl` and its project path to check the
125 MHz selection.

Hardware measurements have not been run for this change.
