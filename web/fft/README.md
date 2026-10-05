# Shared FFT interface

ALPHA250 and Red Pitaya FFT both include `components.mk`. It packages one
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
