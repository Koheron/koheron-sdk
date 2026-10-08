# Measured browser results — 2026-10-08

This is the first owned-stack pass. Further optimization and a new comparison
against both this implementation and the original are in
[CONTINUED_RESULTS.md](CONTINUED_RESULTS.md).

Three alternating original/owned rounds, 180 measured frames per scenario,
30 warmup frames. Chrome 153.0.8010.36, Linux, Intel Core i9-13900, headless,
GPU disabled, 1000 × 500 CSS pixels, DPR 1. TypeScript 5.9.3 compiles both
widgets to ES5; owned assets use locked Terser 5.44.0. The original includes
its existing peak-preserving reduction and batched Canvas hooks.

Values below are medians of the three round medians/p95s, **original → owned**.
All noise spectra vary between eight seeded, prebuilt frames. Acquisition,
unit conversion, peak search and history heatmaps are outside this plot benchmark.
Raw per-round measurements and source hashes are in [results.json](results.json).

| Scenario | Redraw median (ms) | Redraw p95 (ms) | Sampled allocation (KiB/frame) |
| --- | ---: | ---: | ---: |
| fft-linear-4 | 10.3 → 5.5 | 17.4 → 6.9 | 3640.0 → 114.9 |
| pna-log-3 | 10.1 → 8.1 | 14.4 → 9.6 | 1894.5 → 265.1 |
| fft-deep-zoom | 1.6 → 0.8 | 2.3 → 1.2 | 32.6 → 27.4 |
| scope-2 | 3.6 → 2.2 | 5.4 → 4.1 | 729.8 → 213.8 |
| pna-log-y | 7.5 → 5.7 | 9.4 → 8.1 | 1494.7 → 281.8 |

| Scenario | rAF interval median / p95 (ms) | Trusted input → rAF p95 (ms) | Zoom/reset CPU median / p95 (ms) | Zoom/reset → rAF p95 (ms) |
| --- | ---: | ---: | ---: | ---: |
| fft-linear-4 | 33.3 → 16.7 / 33.4 → 33.4 | 55.6 → 30.7 | 29.8 → 18.2 / 47.9 → 43.7 | 78.7 → 62.9 |
| pna-log-3 | 16.7 → 16.7 / 33.4 → 33.4 | 34.3 → 26.0 | 19.2 → 15.7 / 39.6 → 31.8 | 49.2 → 45.2 |
| fft-deep-zoom | 16.7 → 16.7 / 16.7 → 16.8 | 19.1 → 17.6 | 24.9 → 18.9 / 45.6 → 27.5 | 71.0 → 45.9 |
| scope-2 | 33.3 → 16.7 / 50.0 → 33.4 | 53.2 → 37.3 | 15.8 → 15.9 / 22.4 → 25.1 | 52.3 → 53.4 |
| pna-log-y | 16.7 → 16.7 / 16.8 → 16.8 | 32.3 → 29.6 | 14.8 → 13.6 / 18.7 → 22.0 | 28.0 → 40.3 |

FFT redraw CPU fell about 47%, log-X PNA 20%, deep zoom 50%, scope 39% and
log-Y PNA 24%. Sampled spectrum allocation fell 81–97% (deep zoom 16%).
Sampling estimates allocations including collected objects, rather than exact
counts or retained memory. Post-GC retained deltas were small (median roughly 24–134 KiB
for the owned spectrum cases) and are reported per round in the JSON.

Trusted hover handlers themselves stayed around 0.4–0.6 ms; improved input-to-rAF
latency comes mainly from shorter redraw work. These timestamps are responsiveness
proxies, not display presentation measurements. FFT median rAF cadence changed
33.3 → 16.7 ms; its p95 remains 33.4 ms. PNA cadence was usually already 16.7 ms.
The scope zoom/reset median did not improve, and log-Y zoom/reset p95 worsened
28.0 → 40.3 ms. Unrelated C++ builds were running on the host; alternate-order
rounds reduce but do not eliminate that variability. These results establish
measured improvements on this setup, rather than a universal FPS guarantee.

Separate CPU profiles saved in `tmp/plotting/*.cpuprofile` identify column
reduction as the largest remaining named JavaScript cost, followed by XY
normalization and line drawing. Heap profiles drove removal of per-segment
clipping/pixel arrays and buffer-growth copies. Direct unbatched Canvas calls
were retained after scalar endpoint caching showed extra allocations there.

Validation completed:

- `npm test --prefix web/plotting`: deterministic licensed local builds, baseline
  hashes, identical clipped geometry and no pixels differing beyond the test's
  8-level tolerance at DPR 1/2 across five views; narrow peaks/troughs, dense noise,
  gaps, linear/log axes and deep Y zoom; trusted cursor/drag/wheel/reset events;
  resize, hidden/show and legacy fallback shutdown; frozen input, shape changes,
  hook, fill/step/bar fallbacks; actual FFT/PNA full-bin CSV and full-DPI PNG.
- `bash web/tests/run.sh all`: 161 Node test cases and additional FFT fixtures passed.
- Web builds passed for 17 configurations across ALPHA250, ALPHA250-4, ALPHA15
  and Red Pitaya. A fresh Docker build with locked plotting dependencies produced
  byte-identical plotting assets to the native build and built a clean FFT web UI.
- Real-browser checks were run in Chrome. The Chromium CI job is configured;
  hosted CI, Firefox and Safari were not executed in this run.
- Hardware tests were not run. No FPGA/server build or deployment was requested
  for these browser-only changes.
