# Owned Canvas 2D plotting

`src/` owns the Flot 0.8.3 core, readable color helpers, selection, resize,
time, canvas-text and axis-label plugins. `upstream.json` records revisions,
original hashes and the frozen benchmark's exact bytes/URLs. Flot/colorhelpers
are MIT (IOLA/Ole Laursen); axislabels preserves Xuan Luo's MIT relicensing
notice. `licenses/` includes Flot and the benchmark's jQuery fixture licenses.
Every generated plotting asset retains the relevant MIT notice.

From the SDK root:

```sh
npm ci --prefix web
npm ci --prefix web/plotting
npm run build --prefix web/plotting             # tmp/plotting/owned/*.js
make CFG=examples/alpha250/fft/config.mk web    # SDK web Docker image
# Or build without Docker, using the local dependencies:
make CFG=examples/alpha250/fft/config.mk web \
  PLOT_BUILD='node web/plotting/build.cjs' \
  WEB_COMPILE='node web/build.cjs'

npm test --prefix web/plotting                 # real Canvas, events, exports
bash web/tests/run.sh all                      # existing instrument regressions
npm run benchmark --prefix web/plotting -- --frames=180 --rounds=3 --profile
```

The SDK Docker image installs plotting tools from the same npm lock;
rebuild it with `docker build -f web/Dockerfile.web -t koheron-web:node24 web`
after upgrading the build tools. The plotting
npm lock pins the standalone minifier and Playwright driver. No plotting code
is fetched during a build. Make tracks all local sources/licenses and replaces
existing CDN-derived assets when a source changes. Filenames and script order
in instrument HTML are unchanged; non-plotting asset downloads are separate.

Tests/benchmarks use installed `/usr/bin/google-chrome`, or `$CHROME`. To use a
Playwright-managed browser:

```sh
web/plotting/node_modules/.bin/playwright-core install --with-deps chromium
export CHROME=$(node -p 'require("./web/plotting/node_modules/playwright-core").chromium.executablePath()')
```

## Maintenance boundaries

`PlotBasics` keeps the instrument API, colors, legend, axes and exported raw
arrays. `enableBatchedLines()` now enables `lines.batchSize = 32` in Flot instead
of installing draw hooks that temporarily set widths to zero. Dense opaque
lines use overlapping short paths, data-space clipping before axis transforms,
and one transform per shared endpoint. Sparse traces, fills, steps and shadows
use the standard renderer. Batch size is an internal opt-in for opaque
instrument colors; translucent lines should use the standard path.
The line clipper classifies each shared source endpoint once per line pass, skips
clipping for fully visible segments and rejects pairs on the same outside side.
Crossing segments retain the original Y-then-X data-space intersections before
axis transforms. Region codes are local to the draw; changing normalized points
or axis ranges cannot reuse an old classification. No persistent storage is added.

`series.reuseDatapoints` opts the shared widget into normalized XY buffer reuse
and a fused normalization/bounds pass. Raw sample arrays are never copied or
mutated. These internal buffers are mutable across `setData`; retain `data`,
not `datapoints.points`, for captures/exports. Data-processing hooks bypass the
fast path. Finite numeric samples use direct bounds/buffer updates; coercion,
gaps and Infinity/MAX_VALUE sentinels retain the compatibility path.
Reductions reuse one output array per trace, preserve extrema and
gaps, retain boundary neighbors and restore every bin under deep zoom.
Captured reference reductions are recomputed because array identity does not
prove the caller hasn't changed its values. Cursor interpolation searches the
full sorted source grid. Instrument frequency/time grids must remain sorted.

Dense log-X reduction caches frequency-to-column projections per trace. Every
coordinate is checked by value before reuse; range, width, log mode and visible
index bounds also invalidate projections. Y extrema and gaps are recomputed
each frame. Two Float64 buffers use 16 bytes per visible bin, growing to the
trace's visible high-water capacity; removing a trace or disabling reduction
releases its storage. Linear plots allocate no projection storage; sparse zooms
skip projection work and retain any buffer from an earlier dense view.
The PNA reducer stores extrema indices instead of floating-point values in its
flush closure, and reads plot width once for all traces in a redraw.

Range-only redraws use the owned `plot.updateRanges` path to retain canvases,
text caches, normalized XY buffers and event bindings. It updates axis bounds,
rebuilds ticks/layout/legend, clears transient overlays and redraws once. The
widget rebuilds the Plot when configuration/axis mode changes, a public log-Y
formatter changes, a selection drag is active, or multiple axes/axis labels
require the original plugin lifecycle. A new display pixel density rebuilds
the backing canvases too; cached spectra request a redraw when density changes.
It also falls back for older Flot
assets without this method. Keep axis/style changes in the widget setters so
they mark a full rebuild; `updateRanges` handles first-axis bounds and legend
column count only and does not run processOptions hooks again.

ResizeObserver replaces the old global jQuery resize registry and polling.
Callbacks coalesce in one animation frame; shutdown removes observers, timers
and handlers. Explicit jQuery resize events remain supported. Browsers without
ResizeObserver use a local 200 ms fallback and window resize events.

Usage was checked across all `examples/` and shared `web/` sources: FFT variants
use linear/log X spectra; PNA and DPLL use log X, linear/log Y and signed-estimate
point markers; the pulse-generator interface uses the shared
two-channel scope controls. No client uses the old `$.resize` registry. Time and
axislabels scripts remain available under their existing names even though no
current instrument selects a time-mode axis. Core fill/bar/step/point and hook
fallbacks remain for API compatibility. jQuery remains at the public Flot/DOM
boundary and in the rest of the instrument UI.

Performance conditions, metrics, profiling and baseline rules are in
[benchmark/README.md](benchmark/README.md). Measured results are recorded in
[benchmark/RESULTS.md](benchmark/RESULTS.md),
[benchmark/CONTINUED_RESULTS.md](benchmark/CONTINUED_RESULTS.md), the
range-update comparison in [benchmark/RANGE_RESULTS.md](benchmark/RANGE_RESULTS.md),
the extrema-loop comparison in [benchmark/EXTREMA_RESULTS.md](benchmark/EXTREMA_RESULTS.md),
the paired normalization comparison in
[benchmark/NORMALIZATION_RESULTS.md](benchmark/NORMALIZATION_RESULTS.md)
and the latest line-clipping comparison in
[benchmark/CLIPPING_RESULTS.md](benchmark/CLIPPING_RESULTS.md).
