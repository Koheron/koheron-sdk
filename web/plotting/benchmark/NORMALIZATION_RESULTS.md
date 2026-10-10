# Finite-sample normalization — 2026-10-10

The shared widget's normalized XY path now updates bounds and buffer slots
directly for ordinary finite numeric samples. Coerced values, missing samples,
NaN and Infinity retain their existing normalization. Finite ±MAX_VALUE
sentinels also stay on that path so they cannot affect autoscale bounds.
The batch flag is read once per series. No persistent storage is added.

The saved `previous` snapshot is the merged extrema-loop implementation from
PR #843. Separate runs showed large timing swings, including after constraining
CPU affinity, so their absolute median comparisons were inconclusive.
The new paired runner draws both stacks within each animation frame on one
renderer thread, alternating drawing order each frame and positions each round.
This reduces the effect of CPU-frequency/scheduling differences between variants.

Three rounds measured 180 paired frames after 30 warmup frames per scenario.
Chrome 154.0.8037.57, Intel Core Ultra X7 368H, Linux, CPU affinity 0–3,
headless, GPU disabled, DPR 1. Each isolated child has the same 1000 × 500 CSS
pixel plot; the parent viewport is 2400 × 800 to keep both visible.
There is no heap sampling, hover injection or zoom/reset phase in this run.
Raw measurements, source/runner hashes and snapshot hashes are in
[normalization-results.json](normalization-results.json). Commands and the
paired measurement method are documented in [README.md](README.md).

Each median/p95 is the median of three round statistics, **previous → current**.
The paired saving is the average previous-minus-current CPU time across all
540 frame pairs, with drawing order balanced within each round.

| Scenario | Redraw CPU median (ms) | Redraw CPU p95 (ms) | Mean paired CPU saving (ms/frame) |
| --- | ---: | ---: | ---: |
| fft-linear-4 | 1.2 → 1.1 | 1.6 → 1.5 | 0.046 |
| pna-log-3 | 1.1 → 1.1 | 2.1 → 2.0 | 0.069 |
| fft-deep-zoom | 0.2 → 0.2 | 0.5 → 0.5 | 0.013 |
| scope-2 | 0.8 → 0.7 | 0.9 → 0.8 | 0.111 |
| pna-log-y | 0.9 → 0.9 | 2.0 → 2.0 | 0.040 |

Scope redraw median CPU falls approximately 13%; every round favors the new
scope path. Other views show smaller paired savings, with largely unchanged
medians at the timer's 0.1 ms resolution. Combined median rAF cadence remains
16.7 ms. This does not establish an FPS, interaction, zoom or allocation gain.
Both plots share each frame, so absolute times/cadence are not comparable to
the single-plot runs or earlier reports; do not combine their percentages.

Validation:

- Added browser regressions compare the reused fast buffer against generic
  normalization through finite/subnormal values, strings/booleans, missing
  coordinates, NaN, infinities, ±MAX_VALUE and growing/shrinking inputs.
  Frozen source rows remain intact. Batched Infinity gaps and finite sentinel
  point/autoscale behavior have explicit expectations.
- The plotting suite passes segment/sample and five-view Canvas comparisons
  at DPR 1/2, with 0% pixels differing beyond the 8-level tolerance; trusted
  interactions, range updates, resizing, plugin fallbacks and FFT/PNA exports pass.
- All 230 Node tests in `bash web/tests/run.sh all` pass.
- All nine shared-widget instrument web configurations build across ALPHA250,
  ALPHA250-4, ALPHA15 and Red Pitaya.
- Hardware tests and FPGA/server builds were not run. Firefox and Safari were
  not tested.
