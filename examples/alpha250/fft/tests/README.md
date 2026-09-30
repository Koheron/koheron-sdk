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

Checks one-second telemetry polling while controls stay at 4 Hz, frame budgets
including acquisition and drawing, single in-flight acquisition, pause, and
error retry backoff. Comparison tests check reference-cache reuse and
invalidation after unit changes or replacement.
