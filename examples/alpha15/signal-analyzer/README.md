# ALPHA15 signal analyzer

Select this instrument with `CFG=examples/alpha15/signal-analyzer/config.mk`.
V1 instruments require a V1 OS image; the image supplied with the board is V0.

```sh
make doctor CFG=examples/alpha15/signal-analyzer/config.mk
make validate CFG=examples/alpha15/signal-analyzer/config.mk
make web CFG=examples/alpha15/signal-analyzer/config.mk
make -j CFG=examples/alpha15/signal-analyzer/config.mk
make CFG=examples/alpha15/signal-analyzer/config.mk HOST=192.168.1.100 run
```

`run` streams logs. Ctrl+C stops the stream and leaves the instrument running.
`memory.yml` defines the registers, mappings and acquisition parameters.

The interface uses the shared FFT workspace: compact acquisition controls,
responsive spectrum, pause/resume, visible-range peak readout, reference capture,
one-second linear PSD averaging, max hold, spectrogram, occurrence density,
and CSV/PNG exports. Precision DACs use the same digit editors as ALPHA250 FFT:
type and press Enter, or select a digit and use the arrow keys or mouse wheel.
Startup reads DAC settings without writing outputs. Board telemetry runs once
per second and stops on page exit.

ALPHA15 retains ADC 0/1, difference/sum, independent 2 V/8 V input ranges and
internal/external 10 MHz reference selection. Difference/sum requires matching
input ranges; the interface shows a reminder if they differ. The ADC sample
rate is fixed at 15 MS/s; this instrument has no RF DAC generator.

The logarithmic frequency grid stitches the low-frequency decimator, mid-band
decimator and FPGA FFT. Bins use `k * fs / N`, with DC at zero. Overlapping
band-boundary bins are omitted so the grid remains ordered. Exports retain every stitched
bin in Hz. Units are dBV/√Hz, dBV and nV/√Hz; dBV uses each band's window noise
bandwidth. A captured reference keeps its original frequency grid, bandwidth
and input range metadata when acquisition settings change.

The bands have different integration times. Low and mid snapshots are
polled at their frame periods. A stitched spectrum enters the display and
history only when all three server sequence numbers have advanced; repeated
cached spectra do not add history samples or bias density counts. The resulting
update rate is limited by the low-frequency band (about 3.58 frames/s). The
bands are independently timed, so stitched spectra are not simultaneous captures.
Pause stops spectrum requests and freezes display/history while hardware acquisition
continues. Local settings edits suspend spectrum reads until fresh controls
are read back. Replies already in flight are discarded when settings change,
and returned frames keep the metadata associated with their request. Malformed
bands are retried without caching the invalid response.

Channel, window, reference or input range changes restart all three acquisition
bands. The server clears the decimator's 16-frame averages, discards partial
segments, drains queued samples and allows CIC/FIR settling before refilling.
The RF band skips the accumulator period already in progress. No frame is
accepted until all bands publish complete spectra in the new acquisition
generation. Startup and settings changes therefore need about six seconds for
the low-frequency band to settle and refill its average; the interface shows
“Waiting for fresh spectrum…” during this interval. Fresh frames reset live
history; references retain their original settings. These changes require
rebuilding the server as well as the web interface, without FPGA changes.

Density CSV uses frequency/level/count rows in dBV because each band has its
own bandwidth; density exports in noise units use the shared level-by-frequency
format.

## Host checks

The regression suite requires Node.js, `typescript` and `jsdom` (see
`web/package.json`), resolved through `NODE_PATH` if installed outside the repo:

```sh
bash web/tests/run.sh alpha15
g++ -std=c++20 -Wall -Wextra -Werror -I. -Iserver/external_libs -I/usr/include/eigen3 \
    examples/alpha15/signal-analyzer/tests/test_acquisition.cpp \
    -o /tmp/alpha15-test-acquisition
/tmp/alpha15-test-acquisition
```

It mounts the actual templates and shared workspace with simulated board I/O,
and checks startup, channel/window commands, band pacing and frequency order,
voltage conversions, pause, reference metadata, history reset, exports,
connection failure, teardown, in-flight settings changes, malformed band retry,
frame metadata ownership, fresh-band sequence gating, acquisition generations,
and logarithmic rendering reduction. The C++ regression checks clearing and
refilling the 16-frame average, publication readiness and snapshot ownership.
Shared FFT regressions are documented in `examples/alpha250/fft/tests/README.md`.
Precision-DAC transport and editing are shared under `web/precision-channels`;
the temperature and supply readout templates are under `web/temperature-sensor`
and `web/power-monitor`, alongside shared telemetry renderers. Clock, temperature
and supply RPC adapters are also shared and support both Promise and callback
reads. Board polling uses the shared one-second instrument lifecycle. Input-range
and multiband acquisition adapters remain local. CI builds this interface and
runs both the browser suite and sanitized C++ acquisition checks.
These are host checks; numerical calibration, board acquisition and FPGA timing
need separate hardware/build validation.
