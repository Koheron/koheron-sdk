# Optional FFT persistence — 2026-10-10

The FFT spectrum view adds an optional Persistence toggle, off by default.
Only the displayed live trace enters the fading layer. The newest trace,
references, average/max hold, grid and interactive overlays stay sharp.
Decay selects a 0.25–5 s exponential time constant; intensity controls the
strength of each new trail (5–50%). Parameter edits retain existing trails.
Clear trails leaves received history and captures intact. Acquisition changes,
units, ranges and resize reset trails; pause and hidden tabs freeze elapsed
aging. Cached unchanged replies are redrawn while persistence is enabled so
fading continues. Spectra skipped by browser paint are not represented.

Two bounded RGBA canvases retain history pixels and rasterize the next live
trace. The visible history canvas sits behind the regular base canvas and
ignores pointer events. Rasterizing the trace at full opacity before adding it
avoids bright seams at batched stroke boundaries. Smooth CSS opacity decay is
periodically committed to the pixel buffer with a reduction below 1/2, allowing
even the final 8-bit alpha level to clear. Frame tokens prevent reference/cursor
redraws from adding the same spectrum again. Disabling or shutting down clears
and releases the layers. PNG composites history and base only at export and
labels persistence settings; CSV retains raw samples.

## Paired CPU comparison

Both variants use the current owned renderer/widget. `owned` leaves persistence
off; `persistence` enables a 1 s decay and 15% intensity. Three rounds measure
180 paired frames after 30 warmup frames per scenario. Each same-origin child
has a 1000 × 500 CSS pixel plot at DPR 1; the parent viewport is 2400 × 800.
Drawing order alternates each frame and positions swap each round.
Chrome 154.0.8037.57, Intel Core Ultra X7 368H, Linux, CPU affinity 0–3,
headless, GPU disabled. No heap sampling, hover injection or zoom/reset phase.

Each median/p95 is the median of three round statistics, **off → on**. Added
CPU is the mean persistence-minus-owned time over all 540 frame pairs.

| Scenario | Redraw CPU median (ms) | Redraw CPU p95 (ms) | Added paired CPU (ms/frame) |
| --- | ---: | ---: | ---: |
| fft-linear-4 | 1.0 → 3.3 | 1.2 → 3.8 | 2.364 |
| fft-deep-zoom | 0.5 → 2.3 | 0.7 → 2.6 | 1.811 |
| fft-log-4 | 1.0 → 2.7 | 1.4 → 3.3 | 1.780 |

The feature adds roughly 2 ms of timed redraw CPU in these fixtures. Combined
median rAF cadence stays 16.7 ms, which does not establish a complete display
or hardware acquisition throughput result. CSS/browser composition can add
work outside the timed callback. GPU-enabled browsers, other hosts, larger
plots and higher DPR can have different costs. The two canvases add 4,000,000
pixel bytes (about 3.8 MiB) at this size/DPR, or 16,000,000 bytes at DPR 2,
excluding browser/GPU overhead. The default mode allocates no persistence
buffers. This feature adds drawing work; it is not a performance optimization.

Commands and the offline preview are in [README.md](README.md); raw statistics,
source hashes and pixel capacities are in
[persistence-results.json](persistence-results.json).

Validation:

- Persistence browser checks at DPR 1/2 cover elapsed decay, tiny alpha steps,
  pause/resume, same-frame redraws, parameter edits, resets, memory release,
  source preservation and context restoration after a rendering exception.
- The actual FFT controls run against simulated spectra: trusted drag zoom,
  reference capture, history switching, clear, desktop/narrow layouts,
  disposal and full-DPI PNG/CSV exports pass. Screenshots were inspected.
- With persistence off, all seven views match original sample/segment geometry
  and Canvas rendering at DPR 1/2 (0% pixels differing beyond the 8-level
  tolerance). Hook compatibility, interactions, resizing, normalization,
  repeated ranges and original FFT/PNA exports pass. Both asset checks pass.
- All 233 web Node regressions pass. FFT checks and the persistence/export
  browser checks were repeated after the final export changes.
- Nine instrument web configurations build across ALPHA250, ALPHA250-4,
  ALPHA15 and Red Pitaya.
- Hardware tests and FPGA/server builds were not run. Firefox and Safari were
  not tested.
