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

The bands have independent acquisition periods. Low and mid snapshots are
polled at their frame periods; history describes received snapshots, including
cached data, rather than simultaneous or gap-free acquisition. Pause stops
spectrum requests and freezes display/history while hardware acquisition
continues. Channel, window, reference or input range changes reset live history;
the server's decimator averaging can retain older samples during settling.
Density CSV uses frequency/level/count rows in dBV because each band has its
own bandwidth; density exports in noise units use the shared level-by-frequency
format.

## Host checks

The regression suite requires Node.js, `typescript` and `jsdom` (see
`web/package.json`), resolved through `NODE_PATH` if installed outside the repo:

```sh
node --test examples/alpha15/signal-analyzer/tests/test_workspace.cjs
```

It mounts the actual templates and shared workspace with simulated board I/O,
and checks startup, channel/window commands, band pacing and frequency order,
voltage conversions, pause, reference metadata, history reset, exports,
connection failure, teardown and logarithmic rendering reduction. Shared FFT
regressions are documented in `examples/alpha250/fft/tests/README.md`.
These are host checks; numerical calibration, board acquisition and FPGA timing
need separate hardware/build validation.
