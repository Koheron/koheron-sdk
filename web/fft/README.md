# Shared FFT interface

ALPHA250 FFT, Red Pitaya FFT and ALPHA15 signal analyzer include `components.mk`. It packages one
workspace template, stylesheet, lifecycle, transport, acquisition controls,
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
bindings come from `web/clock-generator`, shared with PNA/DPLL. Clock templates
stay board-specific. `bash web/tests/run.sh fft` runs the generic clock/plot and
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
