# Range-update optimization — 2026-10-08

This pass retains the owned Plot for range-only changes. Canvases, tick-text
caches, normalized XY buffers and event bindings survive a zoom/reset; bounds,
ticks, layout and legend are recomputed and transient overlays are cleared.
Mode/style changes, changed public log-Y formatters, active drags, multiple axes,
axis labels and display pixel-density changes retain the full-rebuild path.
Cached spectra request a redraw when the display density changes too.

The `previous` snapshot matches the second pass's widget hash. Three alternating
original/previous/owned rounds measured 180 frames after 30 warmup frames, using
the same eight seeded input frames and five scenarios. Chrome 153.0.8010.36,
Intel Core i9-13900, Linux, headless, GPU disabled, 1000 × 500 CSS pixels,
DPR 1; TypeScript 5.9.3 and locked Terser 5.44.0. Each round separately measured
12 wheel-zoom/double-click-reset operations. Raw measurements and source/snapshot
hashes are in [range-results.json](range-results.json).

Each value below is the median of three round medians/p95s, **second pass →
current**. Unlike the earlier archived runs, zoom/reset now also runs under
16 KiB heap sampling in every variant. Compare within this run; sampling and
changed host load affect absolute timings across runs.

| Scenario | Zoom/reset CPU median (ms) | Zoom/reset CPU p95 (ms) | Zoom allocation (KiB/operation) | Zoom/reset → rAF p95 (ms) |
| --- | ---: | ---: | ---: | ---: |
| fft-linear-4 | 8.8 → 5.4 | 19.5 → 13.6 | 928.5 → 410.4 | 30.2 → 24.1 |
| pna-log-3 | 8.9 → 6.8 | 17.5 → 13.3 | 663.8 → 392.6 | 25.3 → 21.4 |
| fft-deep-zoom | 8.4 → 5.5 | 15.0 → 11.3 | 867.3 → 495.5 | 24.8 → 24.7 |
| scope-2 | 7.3 → 4.2 | 13.3 → 7.9 | 1102.3 → 316.4 | 27.8 → 22.6 |
| pna-log-y | 6.6 → 5.5 | 9.7 → 6.2 | 495.0 → 304.9 | 17.2 → 16.7 |

Zoom/reset median CPU fell 39%, 24%, 35%, 42% and 17%, respectively. Sampled
zoom allocations fell 56%, 41%, 43%, 71% and 38%. The previous log-Y zoom/reset
CPU regression is improved in this paired comparison. Sampling includes
collected allocations and is an estimate rather than exact allocation counts.

| Scenario | Steady redraw median previous → current (ms) | Current redraw p95 (ms) | Trusted hover → rAF p95 previous → current (ms) |
| --- | ---: | ---: | ---: |
| fft-linear-4 | 2.8 → 2.8 | 3.4 | 19.7 → 21.4 |
| pna-log-3 | 2.8 → 2.8 | 3.2 | 20.2 → 19.8 |
| fft-deep-zoom | 0.4 → 0.4 | 0.7 | 17.4 → 17.2 |
| scope-2 | 1.8 → 1.9 | 2.3 | 29.5 → 16.3 |
| pna-log-y | 2.1 → 2.1 | 2.6 | 18.7 → 20.2 |

Steady redraw has no material incremental improvement; scope median moved by
0.1 ms. Median rAF cadence remains 16.7 ms throughout; scope p95 remains 33.3 ms.
Trusted hover tails are mixed, including small FFT/log-Y regressions, so no
general steady-state responsiveness or FPS gain is claimed. Event-to-rAF latency
is a proxy, not physical display presentation. Shared host load was around 3.

In the same run the frozen original's redraw medians were 4.3, 5.0, 0.7, 2.0
and 3.7 ms; its zoom/reset medians were 11.8, 10.8, 11.9, 8.1 and 7.9 ms.
These support end-to-end original/current comparisons without multiplying
percentages from runs with different loads.

No new projection cache is added: existing log-X storage is still 1.50 MiB for
three 32,769-bin traces, 1.00 MiB for two, and zero for linear plots. Normalized
buffers and text caches are retained across range updates; unused text entries
are removed by the existing canvas-text renderer. Post-GC steady JavaScript heap
deltas are recorded separately in the JSON and exclude typed-array backing memory.

Validation:

- `npm test --prefix web/plotting`: licensed deterministic assets; original
  hashes; the existing five-view segment/pixel comparisons at DPR 1/2 (0% pixels
  differing beyond the 8-level tolerance); trusted cursor, selection, wheel and
  reset behavior; resize/show/hide, legacy fallback and normalization checks;
  actual full-resolution FFT/PNA CSV and PNG exports.
- Repeated range tests compare axes, automatic bounds, ticks, label metrics,
  legends, samples and images against the original across five scenarios and
  eight updates at DPR 1/2, including signed point markers, narrow peaks/gaps,
  deep zoom, changed legends and log-Y formatters. Range images meet the same
  less-than-0.1% pixel-difference tolerance.
- Buffer/Plot reuse is verified for range-only changes; mode/formatter changes,
  active-drag cancellation, axis labels and multiple axes exercise full rebuilds.
  Fifty successive range updates do not duplicate selection handlers.
- Live Chrome emulation changes DPR 2 → 1 → 2 and checks redraw requests,
  rebuilt backing canvases and full backing resolution after zoom.
- `bash web/tests/run.sh all`: 163 Node cases and additional FFT fixtures pass.
- The same 17 instrument web configurations build with the owned plotting
  Docker image across ALPHA250, ALPHA250-4, ALPHA15 and Red Pitaya.
- Hardware tests, FPGA/server builds, hosted CI, Firefox and Safari were not run
  locally. The plotting CI job includes these browser range regressions.

Build/test/benchmark commands and snapshot rules are in
[the maintenance guide](../README.md) and [benchmark instructions](README.md).
