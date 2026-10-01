# Host regression tests

Alpha250 and Red Pitaya share `server/drivers/fft/core.hpp` and
`fpga/lib/power_spectral_density.tcl`. Their adapters retain board clocks,
calibration and existing protocol response shapes. Alpha250 is the reference
FFT implementation: both boards use its arithmetic pipeline settings and window
functions, with board-specific ADC width, FFT size and sample rate. Red Pitaya maps
FFT butterflies to LUTs because the direct port exceeds its 80 DSP blocks.
Both retain the reference's 4-multiplier complex arithmetic; Alpha250 keeps
DSP-based butterflies. Red Pitaya uses a nominal 143 MHz AXI fabric clock
(142.857 MHz from the I/O PLL) and a 125 MHz ADC clock.

Run the shared acquisition tests (requires a C++20 compiler and Eigen), the
FPGA elaboration regression (requires Python 3 and Tcl), and the bus-skew
regression (requires Tcl). These tests do not require Vivado:

```sh
bash server/drivers/fft/tests/run.sh
python3 fpga/tests/fft/test_psd.py
tclsh fpga/vivado/tests/single_bit_skew.tcl
```

The FPGA test compares IP properties and connections against the trace captured
from the original Alpha250 script, allowing only the Red Pitaya parameter
changes. The bus-skew test checks that a single timed bit with constant companion
bits is accepted, while multiple live bits, unrelated sources and unconstrained
or failing paths are rejected. These tests do not replace synthesis or timing
checks when changing those properties.

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

DDS and precision DAC editor regression:

```sh
node examples/alpha250/fft/tests/test_web_controls.js
```

Checks that typed frequencies commit on change, invalid edits do not send
commands or move the paired slider, sliders send one live command per input,
DDS edits respect an updated sample-rate limit, and precision DAC values convert
from millivolts to volts only on valid commits. The shared DDS widget uses
these editing rules across instruments.

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
hidden-tab suspension, measured FPS, and error retry backoff. Also checks preservation
of single-bin peaks, minima, missing-data gaps, boundary neighbours and
frequency ordering during rendering reduction, plus full-bin rendering when
zoomed in. Comparison tests check reference-cache reuse and
invalidation after unit changes or replacement.

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

Additional scheduler and worker-stream regressions (requires Node.js 20 and the
`typescript` dependency from `web/package.json`):

```sh
npm --prefix web install --ignore-scripts --no-package-lock
NODE_PATH="$PWD/web/node_modules" node examples/alpha250/fft/tests/test_web_scheduler.js
NODE_PATH="$PWD/web/node_modules" node examples/alpha250/fft/tests/test_web_stream.js
```

These tests cover delayed animation callbacks, worker wire decoding, receive
timestamps, bounded queues, pause/resume, reconnection and response timeouts.
The shared acquisition, FPGA elaboration, bus-skew, scheduler and worker-stream
regressions run in the `sdk-regressions` CI job.
