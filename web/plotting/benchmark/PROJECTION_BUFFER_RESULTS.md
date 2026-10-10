# Projection buffer access — 2026-10-10

Both dense reducers now retain local references to their X/column buffers
after cache preparation, which may grow the underlying storage. Each input
row is read once per loop iteration for X and Y. The hot reuse branch relies
on preparation's reuse flag
rather than checking the cache object again for every bin. Every X coordinate
is still compared by value before reusing a projection; changed coordinates
are recalculated. Projection math, invalidation, extrema/gaps, source arrays
and output order are unchanged. No persistent storage is added.

The `previous` snapshot is the merged clipping implementation from PR #847
(`47b14ccf`). Both variants use the current workload, which adds a four-trace
log X FFT using the spectrum reducer to the existing six scenarios.
Three rounds measured 180 paired frames after 30 warmup frames per scenario.
Chrome 154.0.8037.57, Intel Core Ultra X7 368H, Linux, CPU affinity 0–3,
headless, GPU disabled, DPR 1. Each same-origin child has a 1000 × 500 CSS
pixel plot; the parent viewport is 2400 × 800. Drawing order alternates each
frame and positions swap each round. There is no heap sampling, hover injection
or zoom/reset phase. See [README.md](README.md) for the paired runner and
[projection-buffer-results.json](projection-buffer-results.json) for raw
statistics and source/snapshot hashes.

Each median/p95 is the median of three round statistics, **previous → current**.
The paired saving averages previous-minus-current CPU time across all 540
frame pairs. Negative savings mean the current variant was slower.

| Scenario | Redraw CPU median (ms) | Redraw CPU p95 (ms) | Mean paired CPU saving (ms/frame) |
| --- | ---: | ---: | ---: |
| fft-linear-4 | 1.2 → 1.2 | 1.8 → 1.9 | 0.004 |
| pna-log-3 | 1.5 → 1.4 | 2.1 → 1.9 | 0.101 |
| fft-deep-zoom | 0.5 → 0.5 | 0.7 → 0.7 | −0.003 |
| scope-2 | 0.6 → 0.6 | 0.7 → 0.7 | 0.004 |
| pna-log-y | 1.5 → 1.4 | 1.9 → 1.8 | 0.077 |
| scope-y-zoom | 0.8 → 0.8 | 1.1 → 1.0 | 0.004 |
| fft-log-4 | 1.4 → 1.4 | 1.8 → 1.8 | 0.068 |

Both PNA median redraw CPU times fall approximately 7%. Mean CPU falls
approximately 6% for three-trace PNA, 5% for log Y PNA and 5% for log X FFT.
All three log-view paired means favor the change in every round. Log FFT's
median stays unchanged at the timer's 0.1 ms resolution. Linear FFT, deep zoom
and scope differences are small and mixed across rounds, so this run does not
establish a consistent gain for them. Combined median rAF cadence remains
16.7 ms. This does not establish an FPS, interaction, zoom-operation or
allocation gain. Absolute times/cadence are not comparable to the single-plot
reports, and percentages should not be combined across reports.

Validation:

- The reduction regression now also compares changing Y extrema on a warm
  spectrum cache against the original. Existing cases exercise in-place and
  replaced rows, gaps, growing/shrinking inputs, changed widths/ranges/modes,
  uncached reductions and per-coordinate log transform counts.
- All seven views pass segment/sample and Canvas comparisons at DPR 1/2, with
  0% pixels differing beyond the 8-level tolerance. Clipping, trusted
  interactions, range updates, resizing, normalization, plugin fallbacks and
  actual FFT/PNA CSV/PNG exports pass.
- All 231 Node tests in `bash web/tests/run.sh all` pass.
- All nine shared-widget instrument web configurations build across ALPHA250,
  ALPHA250-4, ALPHA15 and Red Pitaya.
- Hardware tests and FPGA/server builds were not run. Firefox and Safari were
  not tested.
