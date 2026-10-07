# DPLL gain and controller latency experiments

The combined design uses four-clock P/I/I³ gains and a five-clock I² gain.
The added I² register separates the final carry-save reduction from its 59-bit
discarded-bit carry (one additional clock / 4 ns). Direct Fast P/I and accurate
P/I delays are unchanged. The current five-clock multiplier regression checks
55,408 cycles; the controller regression includes the production mixed
four/five-clock pipelines, signed wraparound, enable clearing and gain changes.
Full-instrument timing is reported in the instrument README.


The original PR 782 Fast P + I instrument passed its 250 MHz production build on
2026-10-07, with setup slack +0.004494 ns, hold slack +0.026840 ns and all nine
bus-skew checks passing. The final placement hook was validated by rerunning
physical optimization on the synthesized and routed instrument. Digital paths
are 56 ns for Fast P and 80 ns for Fast I, plus 6 ns fast-filter group delay;
I²/I³ and the phase-noise monitor share the accurate extractor. See the
[current build results](../../README.md#historical-pr-782-fast-p--i-build-2026-10-07)
for that revision's resources, functional checks and hardware-test limits. The
combined PR 780/782 build is reported separately in the instrument README.

These are historical gain-controller measurements, before the shared 24-bit
accurate extractor and manual Fast P + I path. Their register counts and full
instrument timing results describe that earlier build. See the [current
architecture](../../README.md#manual-accurate--fast-p--i) for the production
interfaces and current validation; gain-only arithmetic remains comparable.

The selected table-gain design supports **16 geometric steps per octave**, prepares gain
tables only when settings change, and produces the fast correction **two clocks
(8 ns) earlier** at 250 MHz. The complete two-loop instrument passes routed
timing with the existing board constraints. Coefficient error is at most **0.020372503%**, below the
accepted 0.5% limit.

The table multiplier and controller are now production sources at
[`../../table_gain.v`](../../table_gain.v) and
[`../../table_corrector.v`](../../table_corrector.v), selected by `corrector.tcl`.
The measurements below distinguish the original controller benchmarks from
full-instrument validation. The previously installed instrument remains unchanged.

## Working architecture

`table_gain.v` replaces the sample-rate multiplication and variable octave
shift with small distributed-RAM lookup tables and a fixed addition tree.
Software calculates table contents when a gain changes; the FPGA continues
processing one sample on every 250 MHz clock.

For octave `n = 0..31`, step `j = 0..15` and polarity `s = -1, 0, +1`:

```
coefficient = s * round(2048 * 2^(j/16))
G = coefficient * 2^n
gain = G / 2048
```

The positive Q1.11 coefficients are:

```
2048 2139 2233 2332 2435 2543 2656 2774
2896 3025 3158 3298 3444 3597 3756 3922
```

The 16-step spacing is about 4.43%. The approximation error above describes the
coefficient, separately from the existing output truncation and wraparound.
The mathematical result for signed input `A` is:

```
floor(A * G / 2^(11 + OUTPUT_LOW)) modulo 2^OUTPUT_WIDTH
```

Input samples are split into four-bit chunks. For each gain, software prepares
16 unsigned products `G * address` and 16 signed products
`G * (address < 8 ? address : address - 16)`. The signed table handles the top
chunk, including sign extension for input widths not divisible by four.
Fixed wiring shifts each partial product to its bit position before addition.
The octave is already included in `G`, so there is no sample-rate octave selector.

Each gain has two table banks. Software writes all 32 entries of the inactive
bank, then flips its active-bank bit. This gives an atomic gain update while
samples continue flowing. It does not require fast host writes or stopping the
loop. `corrector_table_tb.v` demonstrates loading and committing the tables.

The successful controller uses **two clocks for P and PI**, and **three clocks
for I2 and I3**. The two-clock implementation registers partway through the
carry-save reduction tree, balancing table-read/reduction work against the final
addition. The wider I2/I3 paths retain three clocks: table read, carry-save
reduction, then final addition/output slice. All paths accept a sample every clock.

`table_corrector.v` also combines the second summing node and fast accumulator
into a single registered three-input addition, using carry-save compression
before the final carry chain:

```
acc2_next = acc2 + first_sum + i2       (modulo 2^32)
```

This removes the separate second-adder register. The final 8 ns saving consists
of one clock removed from the initial P/PI gain stage and one from this summing
node. P and PI remain aligned. The three-clock version of all four table gains
also passes timing with the fused accumulator, but saves only 4 ns. The accumulator
optimization could separately be investigated with the production multipliers.

| Controller path | Existing topology | Mixed gain stages + fused accumulator | Saving |
| --- | ---: | ---: | ---: |
| P or PI directly to fast correction | 6 clocks / 24 ns | 4 clocks / 16 ns | 8 ns |
| P or PI through first accumulator and I2 to fast correction | 10 clocks / 40 ns | 8 clocks / 32 ns | 8 ns |

These counts start at the corrector inputs and exclude detector, converter and
DAC-routing latency. They describe pipeline contributions, not analog settling
or measured ADC-to-DAC latency. Integrator enable/clear behavior, signed
arithmetic and wraparound are retained; removing delay changes the controller's
discrete-time transfer function as intended.

## Comparison with October 4, 2026

The repository at `3f3d5ff7` used 200 MHz, the original detector and three-clock
gain multipliers. Commit `af2d2d2c` removed two detector clocks, and `0a4f4be0`
raised the clock to 250 MHz without changing the controller's cycle count.
The previous deployed build includes those two changes. The mixed-gain design
removes two more controller clocks.

Counted from the ADC data word entering the detector through the fast-DAC mux:

| FPGA path | October 4, 200 MHz | Previous design, 250 MHz | Table gains, 250 MHz | Reduction vs October 4 |
| --- | ---: | ---: | ---: | ---: |
| P directly to fast DAC mux | 180 ns (36 clocks) | 136 ns (34) | 128 ns (32) | 52 ns / 28.9% |
| PI directly to fast DAC mux | 185 ns (37 clocks) | 140 ns (35) | 132 ns (33) | 53 ns / 28.6% |
| P through I2 to fast DAC mux | 200 ns (40 clocks) | 152 ns (38) | 144 ns (36) | 56 ns / 28.0% |
| PI through I2 to fast DAC mux | 205 ns (41 clocks) | 156 ns (39) | 148 ns (37) | 57 ns / 27.8% |

The register counts are: complex mixer 4; boxcar 3 before / 2 now; CORDIC
20 before / 19 now; unwrapping 2 to frequency or 3 to phase; controller 6/10
before versus 4/8 with table gains; DAC mux 1. A pulse-valid simulation of the
generated current CORDIC confirms its 19-stage latency. The detector regression
checks the two-clock old/new difference. The DDS reference generator runs in
parallel and its nine clocks are not added to the ADC disturbance path.

These are clocked pipeline contributions, excluding the additional boxcar
filter group delay, converter/interface delays, external analog plant and
cables. They are not a measured analog round-trip latency. For the direct PI
path, the 53 ns saving comprises 37 ns from the clock increase, 8 ns from the
detector changes and 8 ns from the new controller.

### Adding the RF ADC and DAC pipelines

The ALPHA250 uses an LTC2157-14 ADC and an AD9747 RF DAC. The ADC's
[Rev. B datasheet, page 5](https://www.analog.com/media/en/technical-documentation/data-sheets/21576514fb.pdf)
specifies **six sample clocks** of pipeline latency. The revision history records
the correction from the older five-clock value still shown on the product page.
The [AD9747 datasheet, page 7](https://www.analog.com/media/en/technical-documentation/data-sheets/AD9743_9745_9746_9747.pdf)
specifies **seven clocks in dual-port mode**. `boards/alpha250/drivers/ad9747.hpp`
sets `DATA_CONTROL=0`, selecting that mode, with the DAC in normal output mode.

For the direct PI feedback path:

| Known pipeline contribution | October 4, 200 MHz | Previous design, 250 MHz | Table gains, 250 MHz |
| --- | ---: | ---: | ---: |
| ADC, 6 clocks | 30 ns | 24 ns | 24 ns |
| Detector + controller + DAC mux | 185 ns | 140 ns | 132 ns |
| DAC, 7 clocks | 35 ns | 28 ns | 28 ns |
| **Subtotal** | **250 ns** | **192 ns** | **184 ns** |

This subtotal improves by **58 ns / 23.2%** for the previous design and
**66 ns / 26.4%** with the table gains. The new controller alone saves 8 ns,
or 4.17% of the current converter-inclusive subtotal. For the PI-through-I2
path the corresponding totals are 270, 208 and 200 ns (25.9% total reduction).

This is not yet the complete connector-to-connector delay. FPGA ADC capture
uses `IDDR` in `SAME_EDGE_PIPELINED` mode; DAC output uses an IOB `FDRE` before
the external DAC samples it. Their edge alignment, converter clock-to-data
timing, board analog/filter delay and external loop delay are additional terms.
The four-tap boxcar contributes 1.5 sample periods of group delay beyond its
register latency: 7.5 ns at 200 MHz or 6 ns at 250 MHz. These terms must be
included in a measured phase-delay budget rather than treating the subtotal
as an analog loop measurement.

Koheron's [published 90 ns loopback measurement](https://www.koheron.com/fpga/alpha250-signal-acquisition-generation/)
includes board interfaces and the reference loopback design. The SDK's standard
ALPHA250 starting point inserts two extra registers at the ADC and two at the
DAC; the DPLL directly sources `adc_dac.tcl` and omits those four registers.
Consequently, the published loopback delay cannot be added unchanged to the
DPLL pipeline subtotal or treated as a measurement of this V1 build.

## Build and simulation checks

Measured with Vivado 2025.1 on 2026-10-06 for `xc7z020clg400-2` at a 4 ns period.
All benchmarks add 0.100 ns clock uncertainty, giving about 0.135 ns total
uncertainty. All internal register paths, including configuration writes and
bank changes, are timed; there are no internal false or multicycle paths.
Only package ports outside the registered benchmark boundaries are excluded.

| Routed implementation | Gain clocks | Slice LUTs | FFs including wrapper | DSPs | Setup slack | Hold slack |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Four production gains, mode 0 | 3 | 433 | 916 | 17 | +0.108 ns | +0.035 ns |
| Four table gains, four-bit chunks, mode 18 | 3 | 2910 | 2438 | 0 | +0.123 ns | +0.056 ns |
| Complete table controller, three-clock gains, fused accumulator | 3 | 3072 | 2419 | 0 | +0.067 ns | +0.058 ns |
| **Complete table controller, mixed gain stages, fused accumulator** | **2 / 2 / 3 / 3** | **2938** | **2066** | **0** | **+0.042 ns** | **+0.074 ns** |

The complete-controller benchmark includes actual gain/accumulator feedback
connections, not just isolated gain blocks. Its wrapper differs from the gain
benchmark; FF totals are therefore not directly additive. Each benchmark models
one loop, while the instrument has two loops. LUT usage is the cost of this
solution. These are out-of-context routes; full-instrument routing is still
required, particularly with two loops and their shared configuration wiring.

`corrector_table_tb.v` passes **33,890 checked cycles** against an independent
128-bit signed multiplication and state-update model, covering background table
writes, atomic commits, positive/negative/zero gains, maximum octave, accumulator
wraparound and all integrator enables. A directed impulse reaches fast correction
at cycle 61 with the mixed-gain fused topology versus cycle 63 with three-clock
gains and the separate adder (cycle 62 for three-clock gains with fusion alone).
The independent model checks the separate, fused three-clock, fused two-clock
and fused mixed-stage controllers in parallel. The all-two-clock controller
passes arithmetic but fails routed timing; it is not the selected solution.

`table_vectors.py` exercises every one of the 16 steps, all 32 octaves, both
polarities, zero, signed/chunk boundaries and seeded random inputs. Both the
four-bit three-clock pipeline and balanced two-clock variants pass **49,252
checked cycles** each. It checks all four production output slices against
Python arbitrary-precision multiplication
while updating the inactive bank. The six-bit, two-clock arithmetic variant
passes 147,748 checked cycles, but fails timing and is not the proposed pipeline.

## Other measured architectures

These alternatives are retained for reproducible comparisons. Negative setup
slack means the requested pipeline does not meet 250 MHz.

| Four-gain benchmark | Gain clocks | Slice LUTs | FFs | DSPs | Setup slack |
| --- | ---: | ---: | ---: | ---: | ---: |
| Narrow DSP plus octave selector, mode 6 | 2 | 513 | 593 | 7 | -1.091 ns |
| Narrow DSP plus octave selector, mode 7 | 3 | 504 | 676 | 7 | -0.161 ns |
| Parallel constant products, 12 steps, Q1.11, mode 8 | 2 | 20517 | 4267 | 0 | -1.330 ns |
| Sparse constant products, 12 steps, Q1.7, mode 10 | 2 | 10356 | 3765 | 0 | -0.954 ns |
| Tables, four-bit chunks, mode 12 | 2 | 3224 | 989 | 0 | -0.858 ns |
| Tables, six-bit chunks, mode 14 | 2 | 5055 | 1123 | 0 | -0.678 ns |
| Tables, eight-bit chunks, mode 16 | 2 | 10769 | 1418 | 0 | -1.866 ns |
| Tables, balanced reduction, mode 20 | 2 | 2605 | 1353 | 0 | -0.049 ns |
| Balanced tables with carry-select addition, mode 21 | 2 | 3084 | 1354 | 0 | -1.161 ns |

The Q1.7 12-step constant bank has at most 0.452723% coefficient error, within
the accepted limit, but relaxing precision alone did not solve its timing.
All six narrow-DSP variants pass 35,792 arithmetic vectors and 5,484 additional
zero/power-of-two comparisons against the production RTL. Both constant-bank
precisions pass the same 35,792-vector arithmetic checks at two/three clocks.
These arithmetic passes do not override negative routed timing slack.
The balanced pipeline moves two carry-save levels into the final stage. The
all-two-clock ripple-carry controller misses by 0.217 ns. The
carry-select experiment also failed complete-controller timing (-0.856 ns);
synthesis shared arithmetic and added a second carry chain on its critical path.

## Reproduce

Run from the repository root with Vivado 2025.1 available:

```sh
export DPLL_VIVADO_SETTINGS=/tools/Xilinx/2025.1/Vivado/settings64.sh
bash examples/alpha250/dpll/tests/gain_latency/run-table.sh
DPLL_GAIN_PIPE_STAGES=2 DPLL_GAIN_FINAL_CSA_LEVELS=2 \
  DPLL_GAIN_BENCH_OUT="$PWD/tmp/tests/alpha250-dpll/table-gain/balanced-sim" \
  bash examples/alpha250/dpll/tests/gain_latency/run-table.sh
bash examples/alpha250/dpll/tests/gain_latency/run-corrector.sh
source "$DPLL_VIVADO_SETTINGS"
vivado -mode batch -nolog -nojournal -notrace \
  -source examples/alpha250/dpll/tests/gain_latency/benchmark_corrector.tcl \
  -tclargs 1 tmp/tests/alpha250-dpll/table-corrector/fused-mixed 2 2 0 3
```

The table simulation defaults to four-bit chunks, Q1.11 and three clocks.
`DPLL_GAIN_CHUNK_BITS`, `DPLL_GAIN_FRACTION_BITS`, `DPLL_GAIN_PIPE_STAGES`,
`DPLL_GAIN_FINAL_CSA_LEVELS`, `DPLL_GAIN_CARRY_BLOCK`, `DPLL_GAIN_PHASE_FRAC` and
`DPLL_GAIN_BENCH_OUT` override its settings/output directory.
For the current Q8 interfaces, set `DPLL_GAIN_PHASE_FRAC=8`,
`DPLL_GAIN_PIPE_STAGES=4` (P/I/I³) or `5` (I²), and
`DPLL_GAIN_FINAL_CSA_LEVELS=2`. This checks the
25-bit P input, 40-bit PI input and unchanged fast DSP input against independent
integer products, including exact four/five-clock table and three-clock DSP latencies.
The corrector benchmark's first argument selects fused (1) or separate (0).
Optional final arguments set initial gain stages, final CSA levels, carry-block
width and I2/I3 gain stages. **`2 2 0 3` is the selected mixed pipeline**;
`3 0 0 3` reproduces the slower all-three-clock fallback. `2 2 0` selects the
all-two-clock ripple-carry variant, which fails timing.

For gain-only routes, use `benchmark.tcl` with a mode and output directory;
mode 18 is the successful three-clock table implementation. Modes 20 and 21
compare two-clock balanced reduction with ripple-carry and carry-select addition.
`run.sh` reproduces the initial narrow-DSP investigation (modes 0, 6 and 7 by default).
`run-constant.sh` exercises the constant-bank implementation. Inspect `result.txt`,
`timing.rpt`, individual gain paths, utilization reports and routed checkpoints.
A negative slack is recorded as an experimental result, not a Tcl execution failure.

## Integration and hardware status

This section records the narrower gain-table design tested on 2026-10-06,
before Fast P+I and the shared accurate extractor. Its passing timing and resource
figures do not apply to the current design; see the
[instrument README](../../README.md#full-fpga-build) for current build status.

That version selected P/PI at two clocks and I2/I3 at three clocks,
with the fused fast accumulator. `gain_programmer.v` supplies single-cycle RAM
writes through an acknowledged command toggle. `gain_control.hpp` fills the
inactive bank and waits for the last write acknowledgement before committing.
The bank switch records the coefficient in FPGA status registers, keeping exact
readback across server restarts. Hardware rejects active-bank writes.

The legacy RPCs retain exact signed 32-bit integer gains, command IDs and return
layouts. The new geometric RPC and full-precision readback are documented in the
[instrument README](../../README.md). Both APIs use the existing signed integer
magnitude limits. The RTL experiments also test larger octaves outside this API
range. Negative octaves/sub-unity gains are not implemented.

Gain-table integration checks on 2026-10-06, before the manual P selector:

- The two-controller AXI simulation passes 125,896 cycles against independent
  signed-product/state models, with 4,352 acknowledged transactions, 4,096 RAM
  writes, 128 commits, active-bank rejection, peripheral reset and payload
  changes while a previous command remains asserted.
- Host gain-control checks pass 8,016 cases under address/undefined-behavior
  sanitizers, including every supported geometric setting on all eight gains,
  integer compatibility, delayed acknowledgements, timeout and restart recovery.
- Eleven web tests cover fractional readback, geometric steps, rejection, startup,
  channel isolation, frequency controls and connection lifecycle.
- The ARM server, generated RPC metadata and web assets build successfully.

For that gain-table version, `make -j4 CFG=examples/alpha250/dpll/config.mk all` completed
full synthesis, placement, routing, physical optimization, timing enforcement,
bitstream generation and instrument packaging. The final `system_wrapper`
contains both controllers with the selected parameters and no DSP multipliers
in any of the eight gain paths. The package contains the matching ARM server,
RPC metadata, new web controls and diagrams.

| Full instrument, Vivado 2025.1 | Result |
| --- | ---: |
| ADC/controller clock | 250 MHz |
| Worst setup slack | +0.002444 ns |
| Worst hold slack | +0.042454 ns |
| Setup/hold/pulse-width total negative slack | 0 ns |
| Bus-skew constraints checked | 8, all pass |
| LUTs after routing | 13,237 / 53,200 (24.88%) |
| Flip-flops | 15,047 / 106,400 (14.14%) |
| DSPs | 59 / 220 |
| Block RAM tiles | 16 / 140 |

The eight gain-path setup slacks are +0.0214, +0.0599, +0.0529 and +0.0387 ns
for loop 0 (P/PI/I2/I3), and +0.0750, +0.1391, +0.0954 and +0.1145 ns for loop 1.
The table implementation removes 34 DSPs from the previous full instrument,
at the cost of more LUTs and flip-flops.

The final critical path is the DAC handoff, with a narrow 2.44 ps setup margin.
Automatic placement initially left DAC0 bit 1 failing by 74.6 ps. The production
post-route hook moves its existing mux register within the DAC handoff pblock,
reroutes and repeats physical optimization and hold repair. It adds no clocked
stage and does not relax any timing constraint. That build reproduced
the passing result.

The separate board DAC timing check passes both bitstream phase 0 (setup
+1.002 ns, hold +0.189 ns) and the 56-step startup phase (setup +0.002 ns,
hold +1.189 ns). The 1 ns startup-phase allowance remains in the production
constraints. There are no unclocked registers or unconstrained internal
endpoints. The existing board I/O constraint coverage remains incomplete:
Vivado reports 14 input and 41 output ports without delay constraints. These
results therefore validate the constrained design and do not establish
complete external ADC/DAC timing or analog loop performance.

See the [full build/check commands](../../README.md#full-fpga-build). To repeat
the DAC phase check on the final checkpoint:

```sh
vivado -mode batch -nolog -nojournal -notrace \
  -source boards/alpha250/tests/check_dac_timing.tcl \
  -tclargs tmp/examples/alpha250/dpll/fpga/dpll.runs/impl_1/system_wrapper_postroute_physopt.dcp \
  tmp/tests/alpha250-dpll/full-design/dac-phases
```

No table-gain candidate has been installed. No analog latency, loop lock or
stability measurement has been made. The CORDIC was unchanged for those measurements.
