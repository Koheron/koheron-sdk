# Phase extraction

The selected extractor retains signed **24-bit IQ** and produces **24-bit phase**,
with pi = 2^21 and a 1.498028 µrad phase count. It accepts one sample per clock
and takes **14 clocks (56 ns at 250 MHz)**. The scaled-radian convention reserves
two sign/headroom bits, so the circular phase has 22 meaningful bits.

Three preparation clocks fold the quadrant and normalize into 27-bit coordinates
without discarding Cartesian input bits. Eight CORDIC clocks use a 32-bit angle
accumulator. Three residual-correction clocks then complete the angle: a
256-entry distributed ROM supplies a reciprocal and interpolation slope; one
DSP interpolates the reciprocal; a second DSP multiplies the remaining y and
adds the base angle. The small-angle approximation atan(y/x) ≈ y/x contributes
approximately 0.16 µrad at |y/x| = 1/128. Explicit DSP register placement keeps
this completion to three clocks. Final rounding is nearest with ties toward
positive infinity; output phase is canonical `[-pi, pi)`. Zero IQ produces zero
phase, though its angle is undefined. Reset flushes output-valid metadata.

The selected parameters are `INPUT_WIDTH=24, PHASE_WIDTH=24,
RESIDUAL_CORRECTION=1, PAIR_START=8, COMPACT_PREP=0`. With residual correction,
`ITERATIONS`, `ROTATIONS_PER_CLOCK` and `FUSE_ROUND` describe only the unused
full-CORDIC fallback. The first eight rotations remain individually registered.

Mixer (4), boxcar (2), extraction (14) and phase unwrapping (3) total **23 clocks
(92 ns)**. The direct phase-feedback path adds five DAC-mux clocks, reaching
112 ns. ADC/DAC pipeline delays bring this subtotal to **164 ns**; boxcar group
delay adds 6 ns. Board interfaces, analog delays and the plant add further delay.
These counts do not establish measured connector latency or control bandwidth.

Unwrapping produces 40-bit accumulated phase and 25-bit frequency. The selected
controller uses `PHASE_FRACTION_BITS=8`, retaining the finer scale through every
gain product and accumulator. Final DAC slices remove eight extra fraction bits,
preserving existing gain settings and DAC conventions. Controllers connect to
`phase_feedback`/`freq_feedback`; the `phase`, `freq` and `m_axis_tdata` views
retain existing monitoring and direct phase-DAC units. Monitoring sources are
unchanged.

## Standalone results, 2026-10-06

| Check | Result |
| --- | ---: |
| Independently checked valid output samples | 192,533 |
| Extraction latency / throughput | 14 clocks / one sample per clock |
| Peak / RMS error against continuous atan2 | 1.115 / 0.405 µrad |
| Peak / RMS error before output rounding | 0.405 / 0.088 µrad |
| Routed setup / hold slack at 250 MHz | +0.014 / +0.039 ns |
| LUTs / flip-flops, including registered benchmark wrapper | 1,039 / 1,017 |
| DSPs / block RAM tiles | 2 / 0 |

Routing used Vivado 2025.1, xc7z020clg400-2, a 4 ns clock and 0.100 ns added
clock uncertainty. Only package ports outside the registered benchmark wrapper
are excluded; all internal register paths remain timed. Negative setup or hold
slack fails the benchmark. The 14 ps setup margin is small and does not qualify
placement in the two-loop instrument. **Full-instrument timing is deferred.**

**Hardware tests:** no deployment, lock, stability, analog latency or phase-noise
measurement was performed. Reported angular errors are arithmetic simulation
statistics over the stated vector sets, not measured noise floors or exhaustive
error bounds over all 24-bit input pairs.

## Direct comparison with the PNA CORDIC

The reference is the actual generated PNA vendor core configured by
[`fpga/lib/pna_cordic.tcl`](../../../../../fpga/lib/pna_cordic.tcl): 24-bit IQ,
24-bit phase, `Translate`, `Scaled_Radians`, `Maximum` pipelining and
`Round_Pos_Neg_Inf`. The script verifies its generated configuration before
simulation. Both cores receive identical Cartesian values within the vendor's
unit-circle range. PNA prefiltering and stochastic 24-to-16 phase conversion
are excluded. Measured extraction latency is **14 clocks versus 28 clocks**.

| Same 131,072-point circle, IQ radius approximately 768,000 | Custom | PNA vendor core |
| --- | ---: | ---: |
| Peak error (µrad) | 1.039 | 1.194 |
| RMS error (µrad) | 0.378 | 0.400 |

Across 160,371 matched inputs with radius at least 524,288, peak/RMS error is
1.098/0.366 µrad here versus 1.194/0.386 µrad for the PNA core. All 272,191
matched inputs are checked for output alignment and latency. Very small IQ
magnitudes expose the vendor core's coordinate arithmetic quantization;
normalization improves arithmetic precision there. This does not establish a
hardware noise advantage when the input signal itself has poor signal-to-noise
ratio. References use atan2 of the actual quantized Cartesian inputs.

## Checks

```sh
export DPLL_VIVADO_SETTINGS=/tools/Xilinx/2025.1/Vivado/settings64.sh
bash examples/alpha250/dpll/tests/phase_extraction/run.sh
DPLL_PHASE_ROUTE=1 bash examples/alpha250/dpll/tests/phase_extraction/run.sh
```

`vectors.py` generates 202,033 independent atan2 references, covering every IQ
pair in [-32,32], signed 24-bit extrema, power-of-two boundaries, sixteen
amplitude sweeps, random full-range IQ, lower-eight-bit angle differences,
valid bubbles and resets. The selected test checks exact 14-clock latency,
reset flushing, circular error below 1.5 µrad, pre-rounding error below 0.6 µrad
and agreement within two phase counts with the full-CORDIC fallback. The script
also checks fallback pipeline variants and the two-clock 24-bit boxcar against
independent signed-average references.

The optional PNA comparison requires its generated `system_cordic_0` simulation
VHDL. The default path is the generated ALPHA250-4 PNA project; set
`DPLL_PNA_CORE` to use another generated copy of this core:

```sh
bash examples/alpha250/dpll/tests/phase_extraction/run-pna.sh
```

The comparison writes all aligned phase outputs and a `comparison.json` report.
`DPLL_PNA_TEST_OUT` and `DPLL_PHASE_TEST_OUT` select independent output directories.

`../test_detector.tcl` checks historical vendor detectors bit-for-bit and the
custom detector using a real ADC carrier mixed with a complex DDS reference.
16,084 samples cross phase wraps in both directions and arrive seven clocks
before the original detector. After removing a constant accumulated offset,
phase/frequency differences are bounded to four legacy counts. It checks mixer
orientation, fractional Cartesian bits, fractional phase/frequency after
unwrapping, and exact compatibility slices. This complements the tighter
standalone angular-error test.

`../run-table-system.sh` compares widened products and accumulator states with
independent signed multiplication models while programming both controllers.
Both eight-extra-fraction-bit and legacy zero-extra-bit configurations pass
125,896 cycles, 4,352 acknowledged transactions, 4,096 RAM writes and 128 commits.
`../check_phase_feedback.tcl` validates both production detector/controller
connections without full-instrument synthesis or implementation:

```sh
source "$DPLL_VIVADO_SETTINGS"
vivado -mode batch -nolog -nojournal -notrace \
  -source examples/alpha250/dpll/tests/check_phase_feedback.tcl \
  -tclargs tmp/examples/alpha250/dpll/cores /tmp/dpll-phase-feedback-check
```

## Fallback and experimental configurations

`RESIDUAL_CORRECTION=0, PHASE_WIDTH=24, ITERATIONS=24` selects the full-CORDIC
fallback: 19 clocks, 24 rotations, paired rotations 8–23 and fused rounding.
It passes independent arithmetic checks (1.379 µrad peak / 0.424 µrad RMS).
The `RESIDUAL_CORRECTION=0, PHASE_WIDTH=16, ITERATIONS=16` phase fallback
takes 15 clocks, with 225.3 µrad peak output error and
35.6 µrad peak before rounding. Single-rotation, paired and separate-rounding
fallback pipelines agree bit-for-bit after latency alignment.

`COMPACT_PREP=1` remains experimental and is disabled in production. Its compact
fallback variants pass arithmetic checks; this does not qualify their timing.
`precision_model.py` explores 16/20/22/24 full rotations with 24-bit phase; it
models arithmetic rather than reset flushing, pipeline timing or hardware.

Optional benchmark arguments after rotations-per-clock and output directory
select pair start, compact preparation, fused rounding and residual correction.
For example, the 19-clock full-CORDIC fallback can be routed with:

```sh
source "$DPLL_VIVADO_SETTINGS"
vivado -mode batch -nolog -nojournal -notrace \
  -source examples/alpha250/dpll/tests/phase_extraction/benchmark.tcl \
  -tclargs 2 /tmp/dpll-phase-fallback 8 0 1 0
```
