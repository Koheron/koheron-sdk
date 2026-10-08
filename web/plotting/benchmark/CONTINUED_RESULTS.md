# Further optimization — 2026-10-08

This records the column-cache pass. The next range-update pass is documented in
[RANGE_RESULTS.md](RANGE_RESULTS.md).

This pass targets the log-X reducer identified by the first CPU profiles.
Frequency-to-column projections are cached per trace and checked against every
coordinate by value. Range, width, axis mode and visible indices invalidate the
cache. Y extrema/gaps are always recomputed. The PNA reducer now keeps extrema
indices rather than captured floating-point values; width is queried once per
redraw. Linear reductions remain uncached because the initial experiment showed
no benefit there. Instrument API, raw data and Canvas rendering are preserved.

The saved `previous` snapshot matches the first pass's recorded widget hash.
Three alternating original/previous/owned rounds measured 180 frames after 30
warmup frames, with the same eight seeded input frames and all five scenarios.
Chrome 153.0.8010.36, Intel Core i9-13900, Linux, headless, GPU disabled,
1000 × 500 CSS pixels, DPR 1; TypeScript 5.9.3, locked Terser 5.44.0.
Raw measurements and source/snapshot hashes: [continued-results.json](continued-results.json).

Each value is the median of three round medians/p95s. `Original` is the frozen
CDN stack; `previous` is the first owned implementation, measured again here.
Absolute times differ from [the earlier run](RESULTS.md) because host load
changed. Only comparisons within the same run support the incremental claims.

| Scenario | Redraw median original / previous / current (ms) | Redraw p95 previous → current (ms) | Sampled allocation previous → current (KiB/frame) |
| --- | ---: | ---: | ---: |
| fft-linear-4 | 4.4 / 2.9 / 2.9 | 4.5 → 4.6 | 117.1 → 108.5 |
| pna-log-3 | 5.2 / 4.4 / 3.0 | 7.7 → 4.5 | 270.2 → 123.9 |
| fft-deep-zoom | 0.8 / 0.5 / 0.5 | 0.9 → 0.8 | 27.9 → 27.2 |
| scope-2 | 2.0 / 1.9 / 1.9 | 2.4 → 2.5 | 218.2 → 209.3 |
| pna-log-y | 3.8 / 3.2 / 2.2 | 4.7 → 3.4 | 283.3 → 185.8 |

PNA median redraw improved another 32% and log-Y PNA 31%; their sampled
allocations fell 54% and 34%. Linear FFT, deep zoom and scope showed no material
incremental gain. Allocation sampling varies, so small differences in unchanged
paths are not optimization claims. The separate 60-frame PNA CPU profile's named
reduction samples fell from roughly 223 ms to 107 ms; profiles remain local in
`tmp/plotting/*.cpuprofile`.

| Scenario | Trusted hover → rAF p95 previous → current (ms) | Zoom/reset CPU median / p95 previous → current (ms) | Zoom/reset → rAF p95 previous → current (ms) |
| --- | ---: | ---: | ---: |
| fft-linear-4 | 22.4 → 21.5 | 8.2 → 8.3 / 19.9 → 20.3 | 30.1 → 31.1 |
| pna-log-3 | 24.0 → 20.9 | 9.0 → 8.9 / 17.3 → 16.9 | 30.6 → 25.1 |
| fft-deep-zoom | 17.9 → 16.9 | 9.3 → 7.9 / 15.7 → 15.9 | 26.0 → 25.7 |
| scope-2 | 33.1 → 29.5 | 7.1 → 7.0 / 14.3 → 13.8 | 29.6 → 28.5 |
| pna-log-y | 20.5 → 19.4 | 6.0 → 7.1 / 8.4 → 9.1 | 17.1 → 16.8 |

Median rAF cadence remains 16.7 ms in every scenario; scope p95 remains 33.3 ms.
Shorter CPU work did not increase measured FPS. Log-Y zoom/reset CPU median
worsened 6.0 → 7.1 ms. Range changes invalidate and refill the projections;
the optimization primarily improves steady redraws. Trusted latency is an
event-timestamp-to-next-rAF proxy, not physical display presentation. Shared
host activity (load average about 7 near the end) adds tail variability.

The cache trades persistent memory for less repeated processing: 16 bytes per
visible bin at its allocated high-water capacity, **1.50 MiB for three
32,769-bin traces**, or 1.00 MiB for two. Linear scenarios allocate none.
Removing traces or disabling reduction releases buffers; sparse zooms retain
prior buffers. `projectionCacheBytes` reports this separately because warmed
heap allocation/retention deltas omit initial allocation and JavaScript heap
metrics omit typed-array backing storage. Post-GC JavaScript retained deltas
for current PNA had medians 34 KiB and 18 KiB; these are not total cache memory.

Validation:

- Chrome Canvas checks at DPR 1/2: same clipped segments and samples, 0% pixels
  differing beyond the 8-level tolerance in five views, including dense noise,
  narrow peaks/troughs, gaps, linear/log axes and deep zoom. Trusted cursor,
  selection/wheel/reset, resize/show/hide and legacy fallback checks pass.
- New regressions verify in-place and replaced grid rows, changing Y peaks/gaps,
  shrinking/growing grids, range/width/mode invalidation and actual transform
  reuse. Browser tests compare changed-grid geometry against the original and
  verify that removed traces and disabled reduction release projection storage.
- Actual FFT/PNA full-resolution CSV and PNG exports, frozen caller input,
  normalization, hook and fill/step/bar fallbacks pass.
- `bash web/tests/run.sh all`: 163 Node cases and additional FFT fixtures pass.
- Web builds pass for the same 17 configurations across ALPHA250, ALPHA250-4,
  ALPHA15 and Red Pitaya, using the owned plotting Docker image.
- Hardware tests, FPGA/server builds, hosted CI, Firefox and Safari were not run.

Build/test/benchmark commands and the before-edit snapshot workflow are in
[the maintenance guide](../README.md) and [benchmark instructions](README.md).
