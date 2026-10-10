# Empty-hook allocations — 2026-10-10

All ten Flot hook call sites check the live list before constructing their
payload arrays. Empty lists skip both the caller array and the dispatcher's
`[plot].concat(args)` allocations. Occupied lists use the original dispatcher;
plugin arguments, receiver, arity and live callback order are preserved. Checks
remain inside the per-series loops so callbacks can change hook lists between
series. No persistent storage is added.

The `previous` snapshot is V1 at `77578b1c`, including PR #848's projection
buffer optimization. The subsequent export-only PR #850 does not change any
benchmark source. Chrome 154.0.8037.57, Intel Core Ultra X7 368H, Linux, CPU
affinity 0–3, headless, GPU disabled. Both raw reports include source and
snapshot hashes. See [README.md](README.md) for commands and methodology.

## Static allocation control

The 400 × 200 CSS pixel, DPR 1 plot disables plugins, data, grid and text and
asserts all ten hook lists are empty. Synchronous overlay drawing exercises
the overlay hook site too. Each variant warms for 30 frames, then draws 100
times in each of 60 measured frames. Three rounds alternate variant order:
18,000 measured draws per variant. A 16 KiB heap sampler includes objects
collected by minor and major GC.

| Round | Previous estimated bytes/draw | Current estimated bytes/draw |
| --- | ---: | ---: |
| 1 | 341.9 | 0.0 |
| 2 | 325.5 | 5.5 |
| 3 | 336.4 | 0.0 |
| Mean | 334.6 | 1.8 |

Mean sampled allocated bytes fall approximately 99.5% **in this control**.
This is an estimate, not an allocation count. Zero sampled bytes does not
prove zero allocations. The result does not establish a whole-instrument
allocation, retained-heap or FPS improvement. Raw samples are in
[hook-control-results.json](hook-control-results.json).

## Instrument redraw CPU

Seven instrument views run in two same-origin frames on one renderer thread.
Each has a 1000 × 500 CSS pixel plot, DPR 1; the parent viewport is 2400 × 800.
Three rounds measure 180 paired frames after 30 warmup frames per scenario.
Drawing order alternates each frame and frame positions swap each round.
There is no heap sampling, hover injection or zoom/reset phase.

Each median/p95 is the median of three round statistics, **previous → current**.
The saving averages previous-minus-current CPU time across all 540 frame
pairs. Negative savings mean the current variant was slower.

| Scenario | Redraw CPU median (ms) | Redraw CPU p95 (ms) | Mean paired CPU saving (ms/frame) |
| --- | ---: | ---: | ---: |
| fft-linear-4 | 1.1 → 1.1 | 1.6 → 1.6 | 0.014 |
| pna-log-3 | 1.4 → 1.4 | 1.9 → 2.0 | -0.008 |
| fft-deep-zoom | 0.5 → 0.4 | 0.7 → 0.7 | -0.002 |
| scope-2 | 0.6 → 0.6 | 0.7 → 0.7 | 0.001 |
| pna-log-y | 1.2 → 1.2 | 1.8 → 1.8 | -0.005 |
| scope-y-zoom | 0.4 → 0.4 | 1.0 → 1.0 | 0.002 |
| fft-log-4 | 1.3 → 1.2 | 1.8 → 1.8 | 0.002 |

Mean CPU differences range from approximately −0.6% to +1.2%, with mixed
signs across rounds in every scenario. This run establishes no consistent
instrument redraw CPU gain. Combined median rAF cadence remains 16.7 ms;
no FPS improvement is established. Absolute timings/cadence are not comparable
to the single-plot reports. Raw statistics are in
[hook-paired-results.json](hook-paired-results.json).

Validation:

- Differential browser checks cover all ten hook stages, exact payload
  identities, argument counts, receiver, live listener addition/removal and
  nested overlay dispatch. The caller's hook arrays remain intact.
- All seven instrument views pass segment/sample and Canvas comparisons at
  DPR 1/2, with 0% pixels differing beyond the 8-level tolerance. Clipping,
  trusted interactions, resizing, normalization, plugin fallbacks, repeated
  range updates and actual FFT/PNA CSV/PNG exports pass. The two asset checks
  pass, including deterministic builds and license retention.
- All 233 Node tests in `bash web/tests/run.sh all` pass.
- All nine shared-widget instrument web configurations build across ALPHA250,
  ALPHA250-4, ALPHA15 and Red Pitaya.
- Hardware tests and FPGA/server builds were not run. Firefox and Safari were
  not tested.
