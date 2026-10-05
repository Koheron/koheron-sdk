# Host regression tests

From the repository root, with the SDK Python dependencies installed:

```sh
PYTHONPATH=python python3 examples/alpha250/fft/tests/test_protocol.py
node examples/alpha250/fft/tests/test_web_protocol.js
```

The web test uses the repository's `typescript` dependency. Both protocol tests
exercise the actual client decoder with an eight-field server response, including
internal and external reference selections. The Python test also checks that the
following response remains aligned.

After generating the FFT server headers (`make CFG=examples/alpha250/fft/config.mk
server drivers_json`), check the owned-snapshot API contract:

```sh
g++ -std=c++20 -fsyntax-only -DKOHERON_SERVER_BUILD \
    -I. -Iserver/external_libs -Itmp/examples/alpha250/fft/server \
    examples/alpha250/fft/tests/test_snapshot.cpp
```

These tests do not require or control a board. They do not validate FPGA timing,
FFT numerical accuracy, or acquisition boundaries after a settings change.

Spectrum display regression:

```sh
node examples/alpha250/fft/tests/test_web_plot.js
```

Checks zero-based bin frequencies (including the 40.008544921875 MHz bin), exact
spectrum length, zero-DC peak handling, pause/resume, unit conversion of retained
samples while paused, displayed-frame metadata,
startup auto-scaling after an empty accumulator frame, and cursor interpolation
when a shared plot uses decimation.
It also checks that resizing a paused spectrum redraws retained samples without
another acquisition or changing its captured settings or paused status.

DDS and precision DAC editor regression:

```sh
node examples/alpha250/fft/tests/test_web_controls.js
```

Checks that typed frequencies commit on change, invalid edits do not send
commands or move the paired slider, sliders send one live command per input,
DDS edits respect an updated sample-rate limit, and precision DAC values convert
from millivolts to volts only on valid commits. The shared DDS widget uses
these editing rules across instruments. The web regressions also run
in the `fft-web` CI job.

Export regression:

```sh
node examples/alpha250/fft/tests/test_web_export.js
```

Checks PNG resolution and annotation scale at pixel densities 1 and 2, frame
channel/window/sample-rate labels, and CSV metadata, units and sample values.

Comparison regression:

```sh
node examples/alpha250/fft/tests/test_web_comparison.js
```

Checks visible-range peak search, DC exclusion, empty-bin ranges, paused updates,
reference ownership and independent frequency grids/window correction, capture
and clear, and reference cursor interpolation. Export tests cover both traces
and their separate acquisition metadata in CSV and PNG output.

Performance scheduling regression:

```sh
node examples/alpha250/fft/tests/test_web_performance.js
```

Checks one-second telemetry polling while controls stay at 4 Hz, independent
acquisition pacing, latest-frame replacement, single queued paint, ownership
of waiting samples and metadata, the 60 Hz paint cap on faster monitors, pause,
hidden-tab suspension, measured FPS, and error retry backoff. Checks received
history retention for cached PSD replies, skipped redundant spectrum paints,
latest-frame replacement back to the displayed PSD, and changed-spectrum FPS
while overlays, metadata, history views or resized axes still require painting.
Failed paints retain the last completed frame and retry cached replies without
inflating changed-spectrum FPS.
Also checks preservation of single-bin peaks, minima, missing-data gaps, boundary neighbours and
frequency ordering during rendering reduction, plus full-bin rendering when
zoomed in. Comparison tests check reference-cache reuse and
invalidation after unit changes or replacement.

The FFT interfaces also opt into the shared batched canvas renderer. Its geometry
regressions check clipped segments, sharp extrema, gaps, line styles, cursor
ownership and sparse-zoom fallback:

```sh
node --test examples/alpha250/phase-noise-analyzer/tests/test_rendering.cjs
```

Received history regression:

```sh
node examples/alpha250/fft/tests/test_web_history.js
```

Checks linear-power exponential averaging, max hold, owned history samples and
metadata, reset on incompatible settings, history retention across DDS changes,
50 ms peak rows and real time gaps, density expiry and duration expansion,
bounded frame counts, occupied level ranges, circular heatmap placement,
fractional scrolling between 50 ms rows, frozen pause position,
HiDPI canvas sizing, full-bin spectrogram/density CSV exports, decimal time
boundary stability, fractional interval ages, partial edge rows and frozen
paused export timing.

History interaction checks cover invalid level edits surviving redraw and blur,
Escape recovery, unit labels, plot-only primary-button zoom, selection preview,
pointer cancellation, stale-hover clearing and stationary-hover updates.
