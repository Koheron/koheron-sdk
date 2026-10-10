# Extrema-loop optimization — 2026-10-10

CPU profiles identified visible-bin extrema reduction as the largest remaining
named redraw cost for dense FFT/PNA traces. Both reducers now keep minimum and
maximum Y values in local scalars outside their flush closures, avoiding repeated
lookups of prior extrema rows. The spectrum reducer also reads each bin's Y once.
Extrema indices still select the original rows; column/gap flushes reset those
indices, preserving ties, frequency order and boundary neighbors.

The `previous` snapshot contains the range-update implementation before this
change. Three alternating previous/owned rounds measured 180 frames per scenario
after 30 warmup frames, using the same eight seeded input frames. Chrome
154.0.8037.57, Intel Core Ultra X7 368H, Linux, headless, GPU disabled,
1000 × 500 CSS pixels, DPR 1, Node 24.21.0. Raw timings and source/snapshot hashes
are in [extrema-results.json](extrema-results.json). See
[README.md](README.md) for commands and measurement definitions.

Each value is the median of three round medians/p95s, **previous → current**.

| Scenario | Redraw CPU median (ms) | Redraw CPU p95 (ms) | Sampled allocation (KiB/frame) |
| --- | ---: | ---: | ---: |
| fft-linear-4 | 2.5 → 2.3 | 3.1 → 2.6 | 119.0 → 116.0 |
| pna-log-3 | 3.2 → 3.0 | 3.7 → 3.4 | 126.2 → 127.1 |
| fft-deep-zoom | 0.7 → 0.6 | 0.9 → 0.9 | 28.1 → 28.6 |
| scope-2 | 1.1 → 1.1 | 1.5 → 1.3 | 221.8 → 213.0 |
| pna-log-y | 2.6 → 2.4 | 3.1 → 2.7 | 186.0 → 185.6 |

Dense FFT, PNA and log-Y PNA median redraw CPU fell approximately 8%, 6% and 8%.
Deep zoom skips the changed dense loops and scope skips reduction; small timing
differences in those paths are not optimization claims. Sampled allocations
remain similar; no new persistent storage is introduced. Existing log-X
projection storage remains 1.50 MiB for three 32,769-bin traces or 1.00 MiB for
two; linear plots use none.

Median rAF cadence remains 16.7 ms throughout, so this is a CPU reduction rather
than a measured FPS gain. Zoom/reset results are mixed: PNA median CPU moved
7.9 → 8.2 ms, while FFT p95 moved 9.2 → 11.2 ms and deep-zoom p95 moved
14.5 → 17.5 ms. Trusted hover-to-rAF p95 also remains mixed. No general zoom or
responsiveness improvement is claimed; all measurements are retained in the JSON.

Validation:

- Existing reduction regressions pass across changing grids, peaks, gaps,
  geometry, cache reuse and deep zoom; full-resolution source rows stay intact.
- The plotting browser suite passes segment/sample equivalence and five-view
  Canvas comparisons at DPR 1/2, with 0% pixels differing beyond the 8-level
  tolerance. Trusted interactions, resizing, range updates, plugin fallbacks
  and actual FFT/PNA CSV/PNG exports pass.
- `bash web/tests/run.sh all` passes.
- All nine instrument web configurations selecting the shared widget build:
  ALPHA250/ALPHA250-4/Red Pitaya FFT and PNA, ALPHA15 signal-analyzer,
  ALPHA250 DPLL and Red Pitaya pulse-generator, using the local Node compiler.
- Hardware tests and FPGA/server builds were not run. Firefox and Safari were
  not tested.
