# Shared-endpoint line clipping — 2026-10-10

The line renderer now classifies each source endpoint once per line pass against the visible
data range, retaining its region code for the next adjacent segment. Fully
visible segments skip intersection checks; pairs on the same outside side are
rejected immediately. Crossing segments keep the original Y-min, Y-max, X-min,
X-max intersections before axis transforms. Codes are local to each draw, so
range changes and direct normalized-buffer edits cannot leave stale results.
No persistent storage is added; path batching and joins are unchanged.

The `previous` snapshot is the merged normalization implementation from PR
#846 (`f278a958`). Both variants use the current workload, which adds a narrow
Y scope view (±0.05 over ±1 signals) to the existing five scenarios.
Three rounds measured 180 paired frames after 30 warmup frames per scenario.
Chrome 154.0.8037.57, Intel Core Ultra X7 368H, Linux, CPU affinity 0–3,
headless, GPU disabled, DPR 1. Each same-origin child has a 1000 × 500 CSS
pixel plot; the parent viewport is 2400 × 800. Drawing order alternates each
frame and positions swap each round. There is no heap sampling, hover injection
or zoom/reset phase. See [README.md](README.md) for the paired runner and
[clipping-results.json](clipping-results.json) for raw statistics and hashes.

Each median/p95 is the median of three round statistics, **previous → current**.
The paired saving averages previous-minus-current CPU time across all 540
frame pairs. Negative savings mean the current variant was slower.

| Scenario | Redraw CPU median (ms) | Redraw CPU p95 (ms) | Mean paired CPU saving (ms/frame) |
| --- | ---: | ---: | ---: |
| fft-linear-4 | 1.2 → 1.2 | 1.6 → 1.6 | −0.007 |
| pna-log-3 | 1.5 → 1.5 | 2.0 → 2.0 | 0.028 |
| fft-deep-zoom | 0.5 → 0.5 | 0.8 → 0.7 | 0.005 |
| scope-2 | 0.6 → 0.6 | 0.8 → 0.7 | 0.028 |
| pna-log-y | 1.5 → 1.5 | 1.8 → 1.8 | 0.022 |
| scope-y-zoom | 0.8 → 0.8 | 1.0 → 1.0 | 0.004 |

Scope mean redraw CPU falls approximately 4%, with favorable paired means in
all three rounds (0.049, 0.008 and 0.026 ms/frame saved). Its median remains
unchanged at the timer's 0.1 ms resolution. Other views have mixed changes
across rounds, so this run does not establish a consistent gain for them.
Combined median rAF cadence remains 16.7 ms. This does not establish an FPS,
interaction, zoom-operation or allocation gain. Absolute times/cadence are not
comparable to the single-plot reports, and percentages should not be combined
across reports.

Validation:

- Browser regressions compare every pair of interior, boundary, side and
  corner points against the original clipper, including gaps/re-entry,
  repeated points, extreme finite values and direct normalized-buffer edits.
  Ordinary/batched lines, log transforms, steps, fills and shadows pass at
  DPR 1/2.
- All six views pass segment/sample and Canvas comparisons at DPR 1/2, with
  0% pixels differing beyond the 8-level tolerance. Trusted interactions,
  range updates, resizing, plugin fallbacks and FFT/PNA exports pass.
- All 231 Node tests in `bash web/tests/run.sh all` pass.
- All nine shared-widget instrument web configurations build across ALPHA250,
  ALPHA250-4, ALPHA15 and Red Pitaya.
- Hardware tests and FPGA/server builds were not run. Firefox and Safari were
  not tested.
