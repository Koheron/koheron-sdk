# Benchmark and validation

The frozen original includes the SDK widget at `04dd3408` and its exact CDN
Flot/plugins, including the already-present extrema reduction and batched-line
hooks. Never edit `benchmark/baseline/` to make a comparison pass.

Seeded fixtures prepare eight frames before timing. Noise in live/average/max-hold
spectra varies between frames; captured references stay fixed. Scenarios
cover 16,385-bin FFT with four traces, 32,769-bin log PNA with three traces,
deep FFT zoom, 8,192-sample two-channel scope and log Y PNA. All contain gaps;
spectra include adjacent narrow peaks/troughs. Both variants run with identical
1000 x 500 CSS pixels, DPR 1, headless Chrome and GPU disabled. Each case warms
30 frames. Three rounds alternate variant order. Do not run other builds or
browser suites alongside a performance comparison.

Linux reports also record `cpuAffinity`, the benchmark process's allowed CPU
list. For example, prefix the benchmark command with `taskset -c 0-3` to
constrain both variants to the same cores. Select a CPU list appropriate for
the host; compare variants within that run instead of comparing absolute times
across hosts or affinity settings.

JSON in `tmp/plotting/benchmark.json` records redraw CPU time, actual rAF
intervals, synthetic hover handler/next-frame latency, and trusted mouse
handler/event-timestamp-to-next-rAF latency injected during live drawing, and
separate widget wheel-zoom/double-click-reset redraw/next-rAF timings.
`zoomSampledAllocatedBytesPerOperation` samples the separate zoom/reset phase
at the same 16 KiB interval, including collected objects. Its timing includes
heap sampling in every variant; earlier archived runs measured zoom timing
without that sampler. Compare paired variants within the new run.
Trusted latency includes event queue delay, but excludes automation client
round-trip time. It is a responsiveness proxy, not a display-present timestamp.
Heap sampling at 16 KiB includes collected allocations; estimated bytes/frame
are not exact allocation counts. Retained heap is measured after GC. Optional
`--profile` writes separate Chrome `.cpuprofile` files after timing (open in
DevTools Performance); production function names are retained for profiling.

`--variants=original` runs only the baseline; `--output=path.json` saves another
report. Use medians across rounds and report p95s too. A 16.7 ms frame cadence
can hide a CPU improvement; do not claim higher FPS from shorter redraws alone.
To compare a further optimization against the current owned stack, save it
**before editing**, then build and benchmark the new version:

```sh
npm run snapshot --prefix web/plotting
# Edit sources, then compare in alternating rounds:
npm run benchmark --prefix web/plotting -- --variants=previous,owned --frames=180 --rounds=3 --profile
```

When CPU-frequency variation makes separate runs inconclusive, compare redraw
CPU within each animation frame:

```sh
taskset -c 0-3 npm run benchmark:paired --prefix web/plotting -- --variants=previous,owned --frames=180 --rounds=3
```

This loads two isolated same-origin frames on one renderer thread. Each plot
remains 1000 × 500 CSS pixels at DPR 1; the parent viewport is 2400 × 800 so
both are visible. Drawing order alternates each frame and frame positions swap
each round. The report includes per-variant median/mean/p95 CPU, paired deltas
(in `deltaOrder` order) and combined frame intervals. It uses no heap sampling,
hover injection or zoom/reset phase. Both plots draw in one rAF, so absolute
timings and cadence are not comparable to the single-plot benchmark. Use the
standard runner for allocation and interaction measurements.

The `previous` snapshot lives in `tmp/plotting/comparison/`; taking another
snapshot replaces it. Its timestamp and asset/widget hashes are included in
the report, alongside current source hashes. The frozen `original` remains
unchanged. `projectionCacheBytes` reports persistent typed projection storage
after warmup; sampled allocations and retained deltas exclude that initial
allocation. Report this memory tradeoff with timing results.

The browser suite compares clipped segments and rendered pixels at DPR 1/2,
checks trusted interactions and resizing, plugin fallbacks and original
normalization. Repeated range checks compare axes, ticks, bounds, legends,
samples and rendered pixels against the original at DPR 1/2, including automatic
Y bounds and narrow zooms. They verify range-update buffer reuse, mode/formatter
rebuilds, drag cancellation, event-handler counts and live device-pixel-ratio
changes via Chrome emulation. Export checks invoke the actual FFT/PNA exporters: CSV keeps all
bins outside zoom, references and overlays; PNG keeps backing-canvas resolution.
Screenshots are saved under `tmp/plotting/screenshots/`. These are browser/host
checks, not hardware acquisition or RF measurement tests.
