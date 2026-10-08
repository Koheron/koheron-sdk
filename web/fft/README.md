# Shared FFT interface

ALPHA250 FFT, ALPHA250-4 FFT, Red Pitaya FFT and ALPHA15 signal analyzer include
`components.mk`. It packages one
workspace template, layout stylesheet, lifecycle, transport, acquisition controls,
spectrum/history views, exports and the shared PNA DDS/PM widget and digit inputs.

Each board supplies a small entry point, its FFT response decoder and board
controls. ALPHA250 adds clock selection, telemetry and precision DAC editors;
Red Pitaya adds its fixed ADC rate and uses PNA's half-scale DAC presentation.
The board entry point runs last, after all shared and board classes are defined.

`workspace.html` mounts before the control imports. `FFTWorkspace` then connects,
initializes acquisition and the board controls, and reads generator settings
without writing DACs. Generator discovery has an independent retry. Exit disposes
the plot, control polling, digit editors and client; a cached navigation return
reconnects with a page reload.

See the [FFT interface guide](../../examples/alpha250/fft/README.md) and
[host tests](../../examples/alpha250/fft/tests/README.md). The generator integration
tests mount both board pages with these actual templates and the actual PNA
widget, including failure and teardown paths.

Numeric editors are packaged from `web/inputs`; the ALPHA250 clock adapter and
bindings come from `web/clock-generator`, shared with PNA/DPLL. The 10 MHz reference and ALPHA250/ALPHA250-4 sampling selectors also come from
`web/clock-generator`. `bash web/tests/run.sh fft` runs the generic clock/plot and
FFT browser regressions; `web/fft/tests/run.sh` runs just the FFT suites.

ALPHA15 opts out of the RF DAC generator and supplies a voltage spectrum grid
with per-band bandwidths. Its adapter combines the two decimator snapshots and
FPGA FFT in ascending bin order. The same plot, references, history views and
exports use that grid, with a logarithmic Hz axis and voltage units. Input range
changes participate in the history signature. Precision DAC digit editing is
shared by ALPHA250 and ALPHA15 through `web/precision-channels`; board telemetry
remains board-specific. The temperature readout template is shared under
`web/temperature-sensor`. `bash web/tests/run.sh alpha15` runs the shared controls
and ALPHA15 workspace checks without requiring PNA fixtures.
See `examples/alpha15/signal-analyzer/README.md` for acquisition limitations and
host tests.

ALPHA250-4 uses this same workspace without the RF DAC generator. Its four
input buttons map to the two acquisition engines through a board decoder, which
uses the selected pair's sampling frequency and preserves the existing RPC
protocol. Clock controls, precision digit editors and board presentation are
shared with ALPHA250 through `web/board-controls/alpha-fft.mk`. Board telemetry
is read once per second through the decoder. See
`examples/alpha250-4/fft/README.md` for the acquisition mapping and host checks.

FFT pages opt into `web/instrument` for common headers, buttons, segmented
controls, focus and validation states, connection-status geometry and disclosure
markers. `fft.css` owns the spectrum/sidebar grid, responsive breakpoints and
control sizing. Precision editor styling comes from `web/precision-channels`.

FFT acquisition controls refresh independently of slow board telemetry. ALPHA250
and ALPHA250-4 use `InstrumentPoller` for non-overlapping readbacks, a one-second
delay after each reply, hidden-page suspension, retries and disposal. ALPHA15
uses the same poller in its board controls. Telemetry failures do not delay
channel, window or clock readbacks.
